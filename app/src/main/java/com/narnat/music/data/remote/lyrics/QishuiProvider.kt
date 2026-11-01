package com.narnat.music.data.remote.lyrics

import com.narnat.music.core.lyrics.KrcDecoder
import com.narnat.music.core.lyrics.LrcUtils
import com.narnat.music.data.remote.HttpClient
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive

/**
 * 汽水音乐 Provider。
 *
 * `api.qishui.com` 的 `luna/pc/search/track` 与 `luna/pc/track_v2` 经实测已失效
 * （HTTP 200 + 空 body，`docs/api-probe.md` 第 7 节），因此关键词搜索路径直接返回空结果；
 * 只有 `keyword` 里带 `track_id=`（用户粘贴的汽水分享链接）时才解析分享页
 * `audioWithLyricsOption`。该差异已记入 `docs/task-B-report.md`。
 */
class QishuiProvider(private val http: HttpClient) : LyricsProvider {

    override val name: String = PLATFORM_QISHUI

    override suspend fun fetch(keyword: String): PlatformResult = withContext(Dispatchers.IO) {
        val trackId = extractTrackId(keyword) ?: return@withContext PlatformResult(source = name)
        val draft = LyricsDraft()
        if (!getLyricsViaApi(trackId, draft)) {
            if (!getLyricsViaSharePage(trackId, expectedNameOf(keyword), draft)) {
                return@withContext PlatformResult(source = name)
            }
        }
        var cover: ByteArray? = null
        if (draft.coverUrl.isNotEmpty()) {
            val resp = http.get(draft.coverUrl, mapOf("Accept" to "image/*,*/*;q=0.8"))
            val bytes = resp.bytes
            if (resp.success && bytes != null && bytes.size >= MIN_COVER_BYTES) cover = bytes
        }
        draft.toResult(name, cover)
    }

    private fun extractTrackId(text: String): String? {
        val match = TRACK_ID.find(text) ?: return null
        return match.groupValues[1]
    }

    private fun expectedNameOf(keyword: String): String =
        keyword.replace(Regex("""https?://\S+"""), " ").replace(TRACK_ID, " ").trim()

    private fun getLyricsViaApi(trackId: String, draft: LyricsDraft): Boolean {
        val url = "$TRACK_V2_URL?track_id=$trackId&media_type=track&queue_type="
        val resp = http.get(url, API_HEADERS)
        if (!resp.success || resp.body.isEmpty()) return false
        val root = jsonObject(resp.body) ?: return false
        val lyric = root["lyric"] as? JsonObject ?: return false
        val content = stringOf(lyric["content"]) ?: ""
        if (content.isEmpty()) return false
        val type = stringOf(lyric["type"]) ?: ""
        draft.lyrics = when (type) {
            "lrc" -> content
            "krc" -> KrcDecoder.krcStringToLrc(content)
            else -> KrcDecoder.krcStringToLrc(content).ifEmpty { content }
        }
        draft.hasLyrics = draft.lyrics.isNotEmpty()
        val translations = lyric["lang_translations"] as? JsonObject
        if (translations != null) {
            for ((_, value) in translations) {
                val transData = value as? JsonObject ?: continue
                val transContent = stringOf(transData["content"]) ?: ""
                if (transContent.isEmpty()) continue
                val transType = stringOf(transData["type"]) ?: ""
                val transLrc = if (transType == "krc") KrcDecoder.krcStringToLrc(transContent) else transContent
                if (transLrc.isNotEmpty()) {
                    draft.translation = transLrc
                    draft.hasTranslation = true
                    break
                }
            }
        }
        return draft.hasLyrics
    }

    private fun getLyricsViaSharePage(trackId: String, expectedName: String, draft: LyricsDraft): Boolean {
        val resp = http.get("$SHARE_URL?track_id=$trackId", SHARE_HEADERS)
        if (!resp.success || resp.body.isEmpty()) return false
        var awoJson = extractJsonObject(resp.body, "audioWithLyricsOption")
        if (awoJson.isEmpty()) awoJson = extractJsonObject(resp.body, "audioWithLyrics")
        if (awoJson.isEmpty()) return false

        val awo = jsonObject(awoJson) ?: return false
        val trackName = stringOf(awo["trackName"]) ?: ""
        val artistName = stringOf(awo["artistName"]) ?: ""
        if (trackName.isNotEmpty() && expectedName.isNotEmpty()) {
            if (LrcUtils.nameSimilarity(expectedName, trackName) < MATCH_THRESHOLD) return false
        }
        if (trackName.isNotEmpty()) draft.songName = trackName
        if (artistName.isNotEmpty()) draft.artist = artistName

        val lyrics = awo["lyrics"] as? JsonObject ?: return false
        val sentences = lyrics["sentences"] as? JsonArray ?: return false
        if (sentences.isEmpty()) return false
        val converted = KrcDecoder.krcJsonToLrc(lyrics.toString())
        draft.lyrics = converted.first
        draft.hasLyrics = draft.lyrics.isNotEmpty()
        if (converted.second.isNotEmpty()) {
            draft.translation = converted.second
            draft.hasTranslation = true
        }
        val coverUrl = stringOf(awo["coverURL"]) ?: ""
        if (coverUrl.startsWith("http")) {
            val tilde = coverUrl.indexOf('~')
            draft.coverUrl = if (tilde >= 0) {
                coverUrl.substring(0, tilde) + "~c5_1080x1080.jpg"
            } else {
                coverUrl
            }
        }
        return draft.hasLyrics
    }

    /** 从 HTML 里按花括号配平抠出指定字段名的 JSON 对象（上限 200000 字符，同 C++）。 */
    private fun extractJsonObject(html: String, fieldName: String): String {
        val marker = "\"$fieldName\""
        val idx = html.indexOf(marker)
        if (idx < 0) return ""
        val objStart = html.indexOf('{', idx)
        if (objStart < 0 || objStart > idx + marker.length + 5) return ""
        var brace = 0
        val limit = minOf(objStart.toLong() + MAX_JSON_SCAN, html.length.toLong()).toInt()
        var i = objStart
        while (i < limit) {
            val c = html[i]
            if (c == '{') {
                brace++
            } else if (c == '}') {
                brace--
                if (brace == 0) return html.substring(objStart, i + 1)
            }
            i++
        }
        return ""
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
        private const val TRACK_V2_URL = "https://api.qishui.com/luna/pc/track_v2"
        private const val SHARE_URL = "https://music.douyin.com/qishui/share/track"
        private const val MAX_JSON_SCAN = 200000L
        private const val MATCH_THRESHOLD = 0.2
        private val TRACK_ID = Regex("""track_id=(\d+)""")
        private val API_HEADERS = mapOf(
            "User-Agent" to "LunaPC/3.0.0(290101097)",
            "Content-Type" to "application/json; charset=utf-8",
            "X-Helios" to "SicAACJWDNiSHEX4DSBVXo3+TNXAHXt9Af6CkPaMTmSX1Jcg",
            "X-Medusa" to "GBR+aez8IZPMw6JSzT62GUzbH3GODwMBg7ZESAAAAQk/GduVkAZWRUCTX0SGSLVDDQ"
        )
        private val SHARE_HEADERS = mapOf(
            "Referer" to "https://music.douyin.com/",
            "Accept" to "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "User-Agent" to "Mozilla/5.0 (iPhone; CPU iPhone OS 16_6 like Mac OS X) " +
                "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.6 Mobile/15E148 Safari/604.1"
        )
    }
}
