package com.narnat.music.domain

import com.narnat.music.core.bili.BiliUtils
import com.narnat.music.core.bili.LinkPlan
import com.narnat.music.core.bili.LinkPlan.Target
import com.narnat.music.core.lyrics.LrcUtils
import com.narnat.music.core.mp4.Mp4TagWriter
import com.narnat.music.core.mp4.Mp4Tags
import com.narnat.music.data.audio.M4aMuxer
import com.narnat.music.data.db.MusicFileDao
import com.narnat.music.data.db.MusicFileEntity
import com.narnat.music.data.library.MusicLibraryRepository
import com.narnat.music.data.remote.HttpClient
import com.narnat.music.data.remote.bili.BiliClient
import com.narnat.music.data.remote.lyrics.LyricsAggregator
import com.narnat.music.data.remote.lyrics.PlatformResult
import java.io.File
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import kotlinx.coroutines.withContext

data class DownloadRequest(
    val urlOrKeyword: String,
    val filename: String,
    val platform: String,
    val delayMs: Int
)

data class DownloadProgress(
    val taskId: String,
    val stage: Stage,
    val title: String,
    val downloadedBytes: Long = 0,
    val totalBytes: Long = 0,
    val speedBytesPerSec: Long = 0,
    val message: String = "",
    val libraryId: Long = 0,
    val error: String = ""
) {
    enum class Stage { Queued, Resolving, Downloading, Lyrics, Packaging, Saving, Done, Failed }
}

private typealias Stage = DownloadProgress.Stage

/**
 * 下载流水线编排：解析 → 下载 → 重封装 → 聚合歌词 → 写标签 → 发布入库。
 * 并发上限 3，全程 `Dispatchers.IO`。
 */
class DownloadService(
    private val biliClient: BiliClient,
    private val http: HttpClient,
    private val lyricsAggregator: LyricsAggregator,
    private val library: MusicLibraryRepository,
    private val dao: MusicFileDao,
    private val cacheDir: File,
    private val scope: CoroutineScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
) {

    private val _tasks = MutableStateFlow<List<DownloadProgress>>(emptyList())
    val tasks: StateFlow<List<DownloadProgress>> = _tasks.asStateFlow()

    private val semaphore = Semaphore(MAX_CONCURRENCY)
    private val jobs = ConcurrentHashMap<String, Job>()
    private val requests = ConcurrentHashMap<String, DownloadRequest>()
    private val counter = AtomicInteger(0)

    fun start(req: DownloadRequest): String {
        val taskId = "task_${System.currentTimeMillis()}_${counter.incrementAndGet()}"
        requests[taskId] = req
        upsert(DownloadProgress(taskId, Stage.Queued, req.filename))
        jobs[taskId] = scope.launch { execute(taskId, req) }
        return taskId
    }

    fun clearFinished() {
        val finished = _tasks.value
            .filter { it.stage == Stage.Done || it.stage == Stage.Failed }
            .map { it.taskId }
            .toSet()
        if (finished.isEmpty()) return
        _tasks.update { list -> list.filterNot { it.taskId in finished } }
        for (id in finished) {
            jobs.remove(id)
            requests.remove(id)
        }
    }

    fun retry(taskId: String) {
        val req = requests[taskId] ?: return
        jobs.remove(taskId)?.cancel()
        upsert(DownloadProgress(taskId, Stage.Queued, req.filename))
        jobs[taskId] = scope.launch { execute(taskId, req) }
    }

    private suspend fun execute(taskId: String, req: DownloadRequest) {
        semaphore.withPermit {
            val tempFiles = ArrayList<File>()
            try {
                val resolved = resolveStage(taskId, req)
                val sourceFile = downloadStage(taskId, resolved, tempFiles)

                update(taskId) { it.copy(stage = Stage.Downloading, message = "重封装中") }
                cacheDir.mkdirs()
                val muxed = File(cacheDir, "${sourceFile.nameWithoutExtension}_muxed.m4a")
                val audioFile = if (M4aMuxer.remux(sourceFile, muxed)) {
                    tempFiles.add(muxed)
                    muxed
                } else {
                    update(taskId) { it.copy(message = "重封装失败，沿用原始 fMP4") }
                    sourceFile
                }

                update(taskId) { it.copy(stage = Stage.Lyrics, message = "抓取歌词中") }
                val lyricsResult = fetchLyricsSafely(resolved.lyricsKeyword)

                update(taskId) { it.copy(stage = Stage.Packaging, message = "封装标签中") }
                val tags = buildTags(req, resolved, lyricsResult)
                if (!Mp4TagWriter.write(audioFile, tags)) {
                    update(taskId) { it.copy(message = "标签写入失败，仍继续入库") }
                }

                update(taskId) { it.copy(stage = Stage.Saving, message = "入库中") }
                val libraryId = saveToLibrary(req, resolved, audioFile, tags)

                update(taskId) {
                    it.copy(
                        stage = Stage.Done,
                        libraryId = libraryId,
                        message = "完成",
                        downloadedBytes = audioFile.length()
                    )
                }
            } catch (e: CancellationException) {
                throw e
            } catch (t: Throwable) {
                val reason = t.message ?: t.javaClass.simpleName
                update(taskId) { it.copy(stage = Stage.Failed, message = reason, error = reason) }
            } finally {
                withContext(NonCancellable) {
                    for (file in tempFiles) file.delete()
                }
            }
        }
    }

    private suspend fun resolveStage(taskId: String, req: DownloadRequest): Resolved {
        update(taskId) { it.copy(stage = Stage.Resolving, message = "解析链接中") }
        val resolvedTarget = resolveTarget(req.urlOrKeyword.trim())
        val info = when (resolvedTarget) {
            is Target.Bvid -> biliClient.fetchVideoInfo(resolvedTarget.bvid)
            is Target.Aid -> biliClient.fetchVideoInfoByAid(resolvedTarget.aid)
            is Target.Keyword -> {
                val hit = biliClient.search(resolvedTarget.keyword).firstOrNull()
                    ?: throw IllegalStateException("搜索无结果: ${resolvedTarget.keyword}")
                biliClient.fetchVideoInfo(hit.bvid)
            }
            else -> null
        } ?: throw IllegalStateException("获取视频信息失败")

        val audio = biliClient.getAudioUrl(info.aid, info.cid)
            ?: throw IllegalStateException("获取音频直链失败")
        val displayName = sanitizeFileName(req.filename.ifEmpty { info.title })
        return Resolved(
            title = info.title,
            videoUrl = "https://www.bilibili.com/video/av${info.aid}",
            audioUrls = audio.urls,
            displayName = displayName,
            lyricsKeyword = displayName
        )
    }

    /**
     * 有链接就必须定位到那一个视频；短链先跟随重定向；
     * 链接解析不出来时直接报错，绝不退回关键词搜索。
     */
    private suspend fun resolveTarget(input: String): Target {
        val first = LinkPlan.plan(input)
        if (first !is Target.NeedRedirect) return first
        val finalUrl = withContext(Dispatchers.IO) { http.resolveRedirect(first.url) }
            ?: throw IllegalStateException("短链解析失败: ${first.url}")
        return when (val second = LinkPlan.plan(finalUrl)) {
            is Target.NeedRedirect -> throw IllegalStateException("无法从链接识别视频: $finalUrl")
            else -> second
        }
    }

    private suspend fun downloadStage(
        taskId: String,
        resolved: Resolved,
        tempFiles: MutableList<File>
    ): File {
        cacheDir.mkdirs()
        val dest = File(cacheDir, "${resolved.displayName}_${System.currentTimeMillis()}.m4s")
        update(taskId) {
            it.copy(
                stage = Stage.Downloading,
                title = resolved.title,
                message = "下载音频中",
                downloadedBytes = 0,
                totalBytes = 0,
                speedBytesPerSec = 0
            )
        }
        var success = false
        for (ua in AUDIO_USER_AGENTS) {
            if (success) break
            val headers = mapOf(
                "Referer" to BiliUtils.REFERER,
                "Origin" to BiliUtils.REFERER,
                "User-Agent" to ua
            )
            for (url in resolved.audioUrls) {
                var lastUpdate = 0L
                val startTime = System.currentTimeMillis()
                success = http.download(url, dest, headers) { total ->
                    val now = System.currentTimeMillis()
                    if (now - lastUpdate >= PROGRESS_INTERVAL_MS) {
                        lastUpdate = now
                        val elapsed = (now - startTime).coerceAtLeast(1L)
                        val speed = total * 1000L / elapsed
                        update(taskId) {
                            it.copy(
                                downloadedBytes = total,
                                totalBytes = total,
                                speedBytesPerSec = speed
                            )
                        }
                    }
                }
                if (success) break
            }
        }
        if (!success) throw IllegalStateException("音频下载失败")
        tempFiles.add(dest)
        val size = dest.length()
        update(taskId) {
            it.copy(downloadedBytes = size, totalBytes = size, speedBytesPerSec = 0)
        }
        return dest
    }

    private suspend fun fetchLyricsSafely(keyword: String): PlatformResult = try {
        lyricsAggregator.fetchBest(keyword)
    } catch (e: CancellationException) {
        throw e
    } catch (t: Throwable) {
        PlatformResult(source = "")
    }

    private fun buildTags(
        req: DownloadRequest,
        resolved: Resolved,
        result: PlatformResult
    ): Mp4Tags {
        var lyrics = result.lyrics
        var translation = result.translation
        if (req.delayMs != 0 && result.hasLyrics && lyrics.isNotEmpty()) {
            lyrics = LrcUtils.adjustLyricsTiming(lyrics, req.delayMs)
        }
        if (req.delayMs != 0 && result.hasTranslation && translation.isNotEmpty()) {
            translation = LrcUtils.adjustLyricsTiming(translation, req.delayMs)
        }
        if (result.hasTranslation && translation.isNotEmpty()) {
            lyrics = LrcUtils.mergeBilingualLyrics(lyrics, translation)
        }
        val sign = if (req.delayMs >= 0) "+" else ""
        val narmeta = "${resolved.videoUrl}|${req.platform}|$sign${req.delayMs}|${resolved.displayName}"
        return Mp4Tags(
            title = result.songName.ifEmpty { resolved.displayName },
            artist = result.artist,
            album = result.album,
            lyrics = lyrics,
            cover = result.cover,
            delayMs = req.delayMs,
            narmeta = narmeta
        )
    }

    private suspend fun saveToLibrary(
        req: DownloadRequest,
        resolved: Resolved,
        audioFile: File,
        tags: Mp4Tags
    ): Long {
        val published = library.publishM4a(audioFile, resolved.displayName)
        val nowSec = System.currentTimeMillis() / 1000
        val systemFilename = File(published.absolutePath).name
        val existing = if (tags.title.isNotEmpty() && tags.artist.isNotEmpty()) {
            dao.findBySongArtistPlatformDelay(tags.title, tags.artist, req.platform, req.delayMs)
        } else {
            null
        }
        if (existing != null) {
            if (existing.filePath.isNotEmpty() && existing.filePath != published.absolutePath) {
                library.removeStoredFile(existing)
            }
            dao.update(
                existing.copy(
                    songName = tags.title,
                    artist = tags.artist,
                    album = tags.album,
                    filePath = published.absolutePath,
                    systemFilename = systemFilename,
                    originalFilename = resolved.displayName,
                    fileSize = published.sizeBytes,
                    delayMs = req.delayMs,
                    platform = req.platform,
                    lyrics = tags.lyrics.ifEmpty { null },
                    mediaUri = published.mediaUri,
                    relativePath = MusicLibraryRepository.RELATIVE_FOLDER,
                    mimeType = MusicLibraryRepository.MIME_TYPE,
                    downloadedAt = nowSec,
                    lastUsedAt = nowSec
                )
            )
            return existing.id
        }
        return dao.insert(
            MusicFileEntity(
                songName = tags.title,
                artist = tags.artist,
                album = tags.album,
                filePath = published.absolutePath,
                systemFilename = systemFilename,
                originalFilename = resolved.displayName,
                fileSize = published.sizeBytes,
                delayMs = req.delayMs,
                platform = req.platform,
                inUse = false,
                downloadedAt = nowSec,
                lastUsedAt = nowSec,
                lyrics = tags.lyrics.ifEmpty { null },
                mediaUri = published.mediaUri,
                relativePath = MusicLibraryRepository.RELATIVE_FOLDER,
                mimeType = MusicLibraryRepository.MIME_TYPE
            )
        )
    }

    private fun sanitizeFileName(raw: String): String {
        val sb = StringBuilder(raw.trim().length)
        for (c in raw.trim()) {
            sb.append(if (c in ILLEGAL_CHARS) '_' else c)
        }
        val trimmed = sb.toString().trim()
        val limited = if (trimmed.length > MAX_NAME_LENGTH) trimmed.substring(0, MAX_NAME_LENGTH) else trimmed
        return limited.ifEmpty { "music_${System.currentTimeMillis()}" }
    }

    private fun update(taskId: String, transform: (DownloadProgress) -> DownloadProgress) {
        _tasks.update { list ->
            var changed = false
            val updated = list.map {
                if (it.taskId == taskId) {
                    changed = true
                    transform(it)
                } else {
                    it
                }
            }
            if (changed) updated else list
        }
    }

    private fun upsert(progress: DownloadProgress) {
        _tasks.update { list ->
            val index = list.indexOfFirst { it.taskId == progress.taskId }
            if (index >= 0) {
                val copy = list.toMutableList()
                copy[index] = progress
                copy
            } else {
                list + progress
            }
        }
    }

    private data class Resolved(
        val title: String,
        val videoUrl: String,
        val audioUrls: List<String>,
        val displayName: String,
        val lyricsKeyword: String
    )

    companion object {
        const val MAX_CONCURRENCY = 3
        private const val PROGRESS_INTERVAL_MS = 200L
        private const val MAX_NAME_LENGTH = 80
        private const val ILLEGAL_CHARS = "\\/:*?\"<>|"
        private val AUDIO_USER_AGENTS = listOf(
            "okhttp",
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " +
                "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
        )
    }
}
