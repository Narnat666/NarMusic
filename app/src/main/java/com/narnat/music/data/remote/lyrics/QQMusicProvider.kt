package com.narnat.music.data.remote.lyrics

import com.narnat.music.core.lyrics.LrcUtils
import com.narnat.music.data.remote.HttpClient
import java.util.Base64
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.longOrNull
import kotlinx.serialization.json.put

/**
 * QQ 音乐歌词：`musicu.fcg` 的 `DoSearchForQQMusicDesktop` + `GetPlayLyricInfo`。
 * POST 失败时降级为 `&data=<urlencoded json>` 的 GET。
 * `song.list[*].listenCnt` 已不存在（api-probe 实测），播放量加成恒为 0。
 */
class QQMusicProvider(private val http: HttpClient) : LyricsProvider {

    override val name: String = PLATFORM_QQ

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
        val body = buildJsonObject {
            put("comm", buildJsonObject {
                put("ct", 19)
                put("cv", "1859")
                put("uin", "0")
            })
            put("req", buildJsonObject {
                put("method", "DoSearchForQQMusicDesktop")
                put("module", "music.search.SearchCgiService")
                put("param", buildJsonObject {
                    put("query", keyword)
                    put("num_per_page", 10)
                    put("page_num", 1)
                })
            })
        }.toString()

        val resp = callFcg(body) ?: return false
        val root = jsonObject(resp) ?: return false
        val req = root["req"] as? JsonObject ?: return false
        val data = req["data"] as? JsonObject ?: return false
        val bodyObj = data["body"] as? JsonObject ?: return false
        val song = bodyObj["song"] as? JsonObject ?: return false
        val songs = song["list"] as? JsonArray ?: return false
        if (songs.isEmpty()) return false

        val best = pickBest(keyword, songs)
        draft.songId = stringOf(best["mid"]) ?: ""
        draft.songName = stringOf(best["name"]) ?: ""
        val singers = best["singer"] as? JsonArray
        if (singers != null && singers.isNotEmpty()) {
            draft.artist = stringOf((singers[0] as? JsonObject)?.get("name")) ?: ""
        }
        val album = best["album"] as? JsonObject
        if (album != null) {
            val albumName = stringOf(album["name"]) ?: ""
            if (albumName.isNotEmpty()) {
                draft.album = albumName
                draft.hasAlbum = true
            }
            val albumMid = stringOf(album["mid"]) ?: ""
            if (albumMid.isNotEmpty()) {
                draft.coverUrl = "https://y.gtimg.cn/music/photo_new/T002R800x800M000$albumMid.jpg"
            }
        }
        return true
    }

    private fun pickBest(keyword: String, songs: JsonArray): JsonObject {
        var bestIndex = 0
        if (songs.size > 1) {
            var maxPop = 0L
            for (song in songs) {
                val obj = song as? JsonObject ?: continue
                val pop = (obj["listenCnt"] as? JsonPrimitive)?.longOrNull ?: 0L
                if (pop > maxPop) maxPop = pop
            }
            var bestScore = -1.0
            for ((index, song) in songs.withIndex()) {
                val obj = song as? JsonObject ?: continue
                val songName = stringOf(obj["name"]) ?: ""
                var score = LrcUtils.nameSimilarity(keyword, songName)
                val singers = obj["singer"] as? JsonArray
                if (singers != null && singers.isNotEmpty()) {
                    val artistName = stringOf((singers[0] as? JsonObject)?.get("name"))
                    if (artistName != null) score += 0.2 * LrcUtils.nameSimilarity(keyword, artistName)
                }
                val pop = (obj["listenCnt"] as? JsonPrimitive)?.longOrNull ?: 0L
                if (maxPop > 0L) score += 0.15 * pop.toDouble() / maxPop.toDouble()
                if (score > bestScore) {
                    bestScore = score
                    bestIndex = index
                }
            }
        }
        return songs[bestIndex] as? JsonObject ?: JsonObject(emptyMap())
    }

    private fun getLyrics(draft: LyricsDraft): Boolean {
        if (draft.songId.isEmpty()) return false
        val body = buildJsonObject {
            put("comm", buildJsonObject {
                put("ct", 19)
                put("cv", "1859")
                put("uin", "0")
            })
            put("req", buildJsonObject {
                put("method", "GetPlayLyricInfo")
                put("module", "music.musichallSong.PlayLyricInfo")
                put("param", buildJsonObject {
                    put("songMID", draft.songId)
                    put("trans", 1)
                })
            })
        }.toString()

        val resp = callFcg(body) ?: return false
        val root = jsonObject(resp) ?: return false
        val req = root["req"] as? JsonObject ?: return false
        val data = req["data"] as? JsonObject ?: return false
        val base64Lyric = stringOf(data["lyric"]) ?: ""
        if (base64Lyric.isEmpty()) return false
        val lyric = decodeBase64(base64Lyric)
        if (lyric.isEmpty()) return false
        draft.lyrics = lyric
        draft.hasLyrics = true
        val base64Trans = stringOf(data["trans"]) ?: ""
        if (base64Trans.isNotEmpty()) {
            val translation = decodeBase64(base64Trans)
            if (translation.isNotEmpty()) {
                draft.translation = translation
                draft.hasTranslation = true
            }
        }
        return true
    }

    /** POST 优先，网络/解析失败降级 GET。返回响应体文本。 */
    private fun callFcg(bodyJson: String): String? {
        val headers = mapOf(
            "Referer" to "https://y.qq.com",
            "Origin" to "https://y.qq.com",
            "Content-Type" to "application/json"
        )
        val post = http.postJson(FCG_URL, bodyJson, headers)
        if (post.success && post.body.isNotEmpty()) return post.body
        val getUrl = "$FCG_URL&data=${encodeQuery(bodyJson)}"
        val get = http.get(getUrl, headers)
        if (get.success && get.body.isNotEmpty()) return get.body
        return null
    }

    private fun decodeBase64(text: String): String = try {
        String(Base64.getMimeDecoder().decode(text), Charsets.UTF_8)
    } catch (e: IllegalArgumentException) {
        ""
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
        private const val FCG_URL = "https://u.y.qq.com/cgi-bin/musicu.fcg?format=json"
    }
}
