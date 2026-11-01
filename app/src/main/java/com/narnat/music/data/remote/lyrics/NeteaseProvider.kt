package com.narnat.music.data.remote.lyrics

import com.narnat.music.core.lyrics.LrcUtils
import com.narnat.music.data.remote.HttpClient
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.longOrNull

/**
 * 网易云音乐歌词：`cloudsearch/pc` 搜索 → `song/lyric` 取 LRC 与 tlyric。
 */
class NeteaseProvider(private val http: HttpClient) : LyricsProvider {

    override val name: String = PLATFORM_NETEASE

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
        val url = "$SEARCH_URL?s=${encodeQuery(keyword)}&type=1&offset=0&limit=10"
        val headers = mapOf("Referer" to "http://music.163.com", "Origin" to "http://music.163.com")
        val resp = http.get(url, headers)
        if (!resp.success) return false
        val root = jsonObject(resp.body) ?: return false
        val code = (root["code"] as? JsonPrimitive)?.longOrNull ?: 0L
        if (code != 200L) return false
        val result = root["result"] as? JsonObject ?: return false
        val songs = result["songs"] as? JsonArray ?: return false
        if (songs.isEmpty()) return false

        val best = pickBest(keyword, songs)
        draft.songId = (best["id"] as? JsonPrimitive)?.longOrNull?.toString() ?: ""
        draft.songName = stringOf(best["name"]) ?: ""
        val artists = best["ar"] as? JsonArray
        if (artists != null && artists.isNotEmpty()) {
            draft.artist = stringOf((artists[0] as? JsonObject)?.get("name")) ?: ""
        }
        val album = best["al"] as? JsonObject
        if (album != null) {
            val albumName = stringOf(album["name"]) ?: ""
            if (albumName.isNotEmpty()) {
                draft.album = albumName
                draft.hasAlbum = true
            }
            val picUrl = stringOf(album["picUrl"]) ?: ""
            if (picUrl.isNotEmpty()) {
                val cut = picUrl.indexOf('?')
                val base = if (cut >= 0) picUrl.substring(0, cut) else picUrl
                draft.coverUrl = "$base?param=1000y1000"
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
                val pop = (obj["pop"] as? JsonPrimitive)?.longOrNull ?: 0L
                if (pop > maxPop) maxPop = pop
            }
            var bestScore = -1.0
            for ((index, song) in songs.withIndex()) {
                val obj = song as? JsonObject ?: continue
                val songName = stringOf(obj["name"]) ?: ""
                var score = LrcUtils.nameSimilarity(keyword, songName)
                val artists = obj["ar"] as? JsonArray
                if (artists != null && artists.isNotEmpty()) {
                    val artistName = stringOf((artists[0] as? JsonObject)?.get("name"))
                    if (artistName != null) score += 0.2 * LrcUtils.nameSimilarity(keyword, artistName)
                }
                val pop = (obj["pop"] as? JsonPrimitive)?.longOrNull ?: 0L
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
        val headers = mapOf("Referer" to "http://music.163.com", "Origin" to "http://music.163.com")
        val resp = http.get("$LYRIC_URL?id=${draft.songId}&lv=-1&kv=-1&tv=-1", headers)
        if (!resp.success) return false
        val root = jsonObject(resp.body) ?: return false
        val lrc = root["lrc"] as? JsonObject ?: return false
        val lyric = stringOf(lrc["lyric"]) ?: return false
        if (lyric.isEmpty()) return false
        draft.lyrics = lyric
        draft.hasLyrics = true
        val tlyric = root["tlyric"] as? JsonObject
        if (tlyric != null) {
            val translation = stringOf(tlyric["lyric"]) ?: ""
            if (translation.isNotEmpty()) {
                draft.translation = translation
                draft.hasTranslation = true
            }
        }
        return true
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
        private const val SEARCH_URL = "http://music.163.com/api/cloudsearch/pc"
        private const val LYRIC_URL = "http://music.163.com/api/song/lyric"
    }
}
