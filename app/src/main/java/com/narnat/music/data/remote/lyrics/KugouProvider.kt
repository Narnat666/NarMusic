package com.narnat.music.data.remote.lyrics

import com.narnat.music.core.lyrics.KrcDecoder
import com.narnat.music.core.lyrics.LrcUtils
import com.narnat.music.data.remote.HttpClient
import java.util.Base64
import kotlin.random.Random
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.longOrNull

/**
 * 酷狗音乐歌词：搜索 → lyrics.kugou.com 候选 → 下载 LRC → 再下 KRC 取 `[language:]` 译文。
 */
class KugouProvider(private val http: HttpClient) : LyricsProvider {

    override val name: String = PLATFORM_KUGOU

    override suspend fun fetch(keyword: String): PlatformResult = withContext(Dispatchers.IO) {
        val draft = LyricsDraft()
        if (!searchSong(keyword, draft)) return@withContext PlatformResult(source = name)
        if (!getLyrics(draft)) draft.hasLyrics = false
        var cover: ByteArray? = null
        if (draft.coverUrl.isNotEmpty()) {
            val resp = http.get(draft.coverUrl, mapOf("Accept" to "image/*,*/*;q=0.8"))
            val bytes = resp.bytes
            if (resp.success && bytes != null && bytes.size >= MIN_COVER_BYTES) cover = bytes
        }
        draft.toResult(name, cover)
    }

    private fun searchSong(keyword: String, draft: LyricsDraft): Boolean {
        val url = "$SEARCH_URL?format=json&keyword=${encodeQuery(keyword)}" +
            "&page=1&pagesize=10&showtype=1"
        val resp = http.get(url)
        if (!resp.success) return false
        val root = jsonObject(resp.body) ?: return false
        val status = (root["status"] as? JsonPrimitive)?.longOrNull ?: 0L
        if (status != 1L) return false
        val data = root["data"] as? JsonObject ?: return false
        val songs = data["info"] as? JsonArray ?: return false
        if (songs.isEmpty()) return false

        val best = pickBest(keyword, songs)
        draft.songId = stringOf(best["hash"]) ?: ""
        draft.songName = stringOf(best["songname"]) ?: ""
        draft.artist = stringOf(best["singername"]) ?: ""
        val albumName = stringOf(best["album_name"]) ?: ""
        if (albumName.isNotEmpty()) {
            draft.album = albumName
            draft.hasAlbum = true
        }
        draft.coverUrl = resolveCoverUrl(best)
        if (draft.coverUrl.isEmpty()) {
            val albumId = stringOf(best["album_id"]) ?: ""
            if (albumId.isNotEmpty()) draft.coverUrl = fetchAlbumCover(albumId)
        }
        return true
    }

    private fun pickBest(keyword: String, songs: JsonArray): JsonObject {
        var bestIndex = 0
        if (songs.size > 1) {
            var maxPop = 0L
            for (song in songs) {
                val obj = song as? JsonObject ?: continue
                val pop = (obj["play_count"] as? JsonPrimitive)?.longOrNull ?: 0L
                if (pop > maxPop) maxPop = pop
            }
            var bestScore = -1.0
            for ((index, song) in songs.withIndex()) {
                val obj = song as? JsonObject ?: continue
                val songName = stringOf(obj["songname"]) ?: ""
                var score = LrcUtils.nameSimilarity(keyword, songName)
                val singer = stringOf(obj["singername"])
                if (singer != null) score += 0.2 * LrcUtils.nameSimilarity(keyword, singer)
                val pop = (obj["play_count"] as? JsonPrimitive)?.longOrNull ?: 0L
                if (maxPop > 0L) score += 0.15 * pop.toDouble() / maxPop.toDouble()
                if (score > bestScore) {
                    bestScore = score
                    bestIndex = index
                }
            }
        }
        return songs[bestIndex] as? JsonObject ?: JsonObject(emptyMap())
    }

    private fun resolveCoverUrl(song: JsonObject): String {
        val albumImg = stringOf(song["album_img"])
        if (albumImg != null && albumImg.startsWith("http")) {
            val cleaned = albumImg.replace(" ", "")
            return if (cleaned.contains("_150.jpg")) {
                cleaned.replace(Regex("""_150\.jpg$"""), "_500.jpg")
            } else {
                cleaned
            }
        }
        val img = stringOf(song["img"])
        if (img != null && img.startsWith("http")) return img.replace(" ", "")
        return ""
    }

    private fun fetchAlbumCover(albumId: String): String {
        val resp = http.get("$ALBUM_URL?albumid=$albumId")
        if (!resp.success) return ""
        val root = jsonObject(resp.body) ?: return ""
        val status = (root["status"] as? JsonPrimitive)?.longOrNull ?: 0L
        if (status != 1L) return ""
        val data = root["data"] as? JsonObject ?: return ""
        val raw = stringOf(data["imgurl"])?.takeIf { it.startsWith("http") }
            ?: stringOf(data["sizable_cover"])?.takeIf { it.isNotEmpty() }
            ?: return ""
        return raw.replace(" ", "").replace("{size}", "800")
    }

    private fun getLyrics(draft: LyricsDraft): Boolean {
        if (draft.songId.isEmpty() && draft.songName.isEmpty()) return false
        val searchKeyword = when {
            draft.artist.isNotEmpty() && draft.songName.isNotEmpty() -> "${draft.artist} - ${draft.songName}"
            draft.songName.isNotEmpty() -> draft.songName
            else -> draft.songId
        }
        val headers = mapOf(
            "Referer" to "http://www.kugou.com",
            "Cookie" to "kg_mid=${randomHex32()}"
        )
        val searchUrl = "$LYRIC_SEARCH_URL?ver=1&man=yes&client=pc" +
            "&keyword=${encodeQuery(searchKeyword)}&duration=200000" +
            "&hash=${encodeQuery(draft.songId)}"
        val resp = http.get(searchUrl, headers)
        if (!resp.success) return false
        val root = jsonObject(resp.body) ?: return false
        val candidates = root["candidates"] as? JsonArray ?: return false
        if (candidates.isEmpty()) return false
        val candidate = candidates[0] as? JsonObject ?: return false
        val id = stringOf(candidate["id"]) ?: return false
        val accessKey = stringOf(candidate["accesskey"]) ?: return false

        val lrcResp = http.get(
            "$LYRIC_DOWNLOAD_URL?ver=1&client=pc&id=$id&accesskey=$accessKey&fmt=lrc&charset=utf8",
            headers
        )
        if (lrcResp.success) {
            val lrcRoot = jsonObject(lrcResp.body)
            val content = lrcRoot?.let { stringOf(it["content"]) } ?: ""
            if (content.isNotEmpty()) {
                draft.lyrics = decodeBase64(content).removePrefix("\uFEFF")
                draft.hasLyrics = draft.lyrics.isNotEmpty()
            }
        }
        if (!draft.hasLyrics) return false

        // download 响应已无 trans 字段（api-probe 实测），翻译只存在于 KRC 的 [language:] 段。
        val krcResp = http.get(
            "$LYRIC_DOWNLOAD_URL?ver=1&client=pc&id=$id&accesskey=$accessKey&fmt=krc&charset=utf8",
            headers
        )
        if (krcResp.success) {
            val krcRoot = jsonObject(krcResp.body)
            val krcBase64 = krcRoot?.let { stringOf(it["content"]) } ?: ""
            if (krcBase64.isNotEmpty()) {
                val raw = decodeBase64Bytes(krcBase64)
                if (raw.isNotEmpty()) {
                    val decoded = KrcDecoder.decryptKrc(raw)
                    if (decoded != null && decoded.second.isNotEmpty()) {
                        draft.translation = decoded.second
                        draft.hasTranslation = true
                    }
                }
            }
        }
        return true
    }

    private fun randomHex32(): String {
        val sb = StringBuilder(32)
        repeat(32) { sb.append(HEX[Random.nextInt(16)]) }
        return sb.toString()
    }

    private fun decodeBase64(text: String): String {
        val bytes = decodeBase64Bytes(text)
        return if (bytes.isEmpty()) "" else String(bytes, Charsets.UTF_8)
    }

    private fun decodeBase64Bytes(text: String): ByteArray = try {
        Base64.getMimeDecoder().decode(text)
    } catch (e: IllegalArgumentException) {
        ByteArray(0)
    }

    private fun jsonObject(text: String): JsonObject? = try {
        Json.parseToJsonElement(text) as? JsonObject
    } catch (e: IllegalArgumentException) {
        null
    } catch (e: kotlinx.serialization.SerializationException) {
        null
    }

    private fun stringOf(element: JsonElement?): String? {
        val primitive = element as? JsonPrimitive ?: return null
        if (!primitive.isString) return null
        return primitive.content
    }

    companion object {
        private const val SEARCH_URL = "http://mobilecdn.kugou.com/api/v3/search/song"
        private const val ALBUM_URL = "http://mobilecdn.kugou.com/api/v3/album/info"
        private const val LYRIC_SEARCH_URL = "http://lyrics.kugou.com/search"
        private const val LYRIC_DOWNLOAD_URL = "http://lyrics.kugou.com/download"
        private const val HEX = "0123456789abcdef"
    }
}
