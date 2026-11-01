package com.narnat.music.data.remote.lyrics

import android.util.Log
import com.narnat.music.core.lyrics.LrcUtils
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope

/**
 * 多平台歌词聚合：并行抓取 + C++ 版三级择优 + 缺项补齐。
 *
 * 三级策略（同 `lyrics_aggregator.cpp::fetchBest`）：
 * 1. 指定平台有核心数据（中文歌=歌词；非中文歌=歌词+翻译）→ 直接选。
 * 2. 按平台优先级找第一个「核心数据 + 封面」齐全的平台。
 * 3. 全都不完整 → 按 `missingScore` 取最高的作为主平台，再补齐缺项。
 */
class LyricsAggregator(
    private val providers: List<LyricsProvider>,
    var preferredPlatform: String = "",
    private val dispatcher: CoroutineDispatcher = Dispatchers.IO,
    private val logger: (String) -> Unit = { message -> Log.i(TAG, message) }
) {

    suspend fun fetchBest(keyword: String): PlatformResult = coroutineScope {
        if (providers.isEmpty()) {
            logger("无歌词提供者")
            return@coroutineScope PlatformResult(source = "")
        }
        val deferred = providers.map { provider ->
            async(dispatcher) {
                try {
                    provider.fetch(keyword)
                } catch (e: CancellationException) {
                    throw e
                } catch (e: Throwable) {
                    logger("${provider.name} 异常: ${e.message}")
                    PlatformResult(source = provider.name)
                }
            }
        }
        selectBest(keyword, deferred.awaitAll())
    }

    fun selectBest(keyword: String, allData: List<PlatformResult>): PlatformResult {
        if (allData.isEmpty()) return PlatformResult(source = "", songName = keyword)
        val platformOrder = buildPlatformOrder(preferredPlatform)

        if (preferredPlatform.isNotEmpty()) {
            val candidate = findPlatform(allData, preferredPlatform)
            if (candidate != null && hasCoreData(candidate)) {
                logger("第一级: 指定平台 $preferredPlatform 核心数据齐全")
                return supplementMissing(cleanCopy(candidate), allData, platformOrder)
            }
            logger("第一级: 指定平台 $preferredPlatform 核心数据不完整，跳过")
        }

        for (platform in platformOrder) {
            val candidate = findPlatform(allData, platform)
            if (candidate != null && isComplete(candidate)) {
                logger("第二级: 整体选取平台 $platform (完整)")
                return supplementMissing(cleanCopy(candidate), allData, platformOrder)
            }
        }

        logger("第三级: 所有平台均不完整，启用兜底拼接策略")
        var primary: PlatformResult? = null
        var bestScore = -1
        for (platform in platformOrder) {
            val candidate = findPlatform(allData, platform) ?: continue
            val score = missingScore(candidate)
            if (score > bestScore) {
                bestScore = score
                primary = candidate
            }
        }
        if (primary == null) {
            logger("所有平台均无有效数据")
            return PlatformResult(source = "", songName = keyword)
        }
        return supplementMissing(cleanCopy(primary), allData, platformOrder)
    }

    fun hasCoreData(data: PlatformResult): Boolean {
        if (!data.hasLyrics || data.lyrics.isEmpty()) return false
        if (!LrcUtils.isChineseLyrics(data.lyrics) &&
            (!data.hasTranslation || data.translation.isEmpty())
        ) {
            return false
        }
        return true
    }

    fun isComplete(data: PlatformResult): Boolean = hasCoreData(data) && data.hasCover

    fun missingScore(data: PlatformResult): Int {
        if (!data.hasLyrics || data.lyrics.isEmpty()) return 0
        var score = 100
        val hasTranslation = data.hasTranslation && data.translation.isNotEmpty()
        if (LrcUtils.isChineseLyrics(data.lyrics)) {
            if (hasTranslation) score += 50
        } else {
            if (hasTranslation) score += 100
        }
        if (data.album.isNotEmpty()) score += 10
        if (data.hasCover) score += 1
        return score
    }

    fun buildPlatformOrder(preferred: String): List<String> {
        if (preferred.isEmpty()) return DEFAULT_ORDER
        val order = ArrayList<String>(DEFAULT_ORDER.size)
        order.add(preferred)
        for (platform in DEFAULT_ORDER) {
            if (platform != preferred) order.add(platform)
        }
        return order
    }

    private fun cleanCopy(data: PlatformResult): PlatformResult =
        if (data.lyrics.isEmpty()) data else data.copy(lyrics = LrcUtils.cleanLyrics(data.lyrics))

    private fun findPlatform(allData: List<PlatformResult>, platform: String): PlatformResult? =
        allData.firstOrNull { it.source == platform }

    private fun supplementMissing(
        bestIn: PlatformResult,
        allData: List<PlatformResult>,
        platformOrder: List<String>
    ): PlatformResult {
        var best = bestIn

        if (!best.hasCover) {
            for (data in allData) {
                val cover = data.cover
                if (data.hasCover && cover != null && cover.isNotEmpty()) {
                    best = best.copy(cover = cover, hasCover = true)
                    logger("补封面: 从 ${data.source} 补充")
                    break
                }
            }
        }

        if (best.album.isEmpty()) {
            for (data in allData) {
                if (data.album.isNotEmpty()) {
                    best = best.copy(album = data.album, hasAlbum = true)
                    logger("补专辑: 从 ${data.source} 补充")
                    break
                }
            }
        }

        if (best.artist.isEmpty()) {
            for (data in allData) {
                if (data.artist.isNotEmpty()) {
                    best = best.copy(artist = data.artist)
                    break
                }
            }
        }

        if (best.songName.isEmpty()) {
            for (data in allData) {
                if (data.songName.isNotEmpty()) {
                    best = best.copy(songName = data.songName)
                    break
                }
            }
        }

        if (!best.hasLyrics || best.lyrics.isEmpty()) {
            for (platform in platformOrder) {
                val data = findPlatform(allData, platform)
                if (data != null && data.hasLyrics && data.lyrics.isNotEmpty()) {
                    val hasTrans = data.hasTranslation && data.translation.isNotEmpty()
                    best = best.copy(
                        lyrics = LrcUtils.cleanLyrics(data.lyrics),
                        hasLyrics = true,
                        translation = if (hasTrans) data.translation else best.translation,
                        hasTranslation = best.hasTranslation || hasTrans
                    )
                    logger("补歌词: 从 ${data.source} 补充")
                    break
                }
            }
        }

        if (best.hasLyrics && !best.hasTranslation && !LrcUtils.isChineseLyrics(best.lyrics)) {
            for (platform in platformOrder) {
                val data = findPlatform(allData, platform)
                if (data != null && data.hasTranslation && data.translation.isNotEmpty()) {
                    best = best.copy(translation = data.translation, hasTranslation = true)
                    logger("补翻译: 从 ${data.source} 补充 (可能与歌词不同源)")
                    break
                }
            }
        }

        return best
    }

    companion object {
        private const val TAG = "LyricsAgg"

        val DEFAULT_ORDER: List<String> =
            listOf(PLATFORM_KUGOU, PLATFORM_NETEASE, PLATFORM_QQ, PLATFORM_QISHUI)
    }
}
