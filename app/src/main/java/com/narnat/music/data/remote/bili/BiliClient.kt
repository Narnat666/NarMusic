package com.narnat.music.data.remote.bili

import com.narnat.music.core.bili.BiliUtils
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
 * B 站接口客户端，行为对齐 `infrastructure/bilibili/bilibili_client.cpp`，
 * 端点/必需头按 `docs/api-probe.md` 的实测结论。
 */
class BiliClient(private val http: HttpClient) {

    data class AidCid(val aid: Long, val cid: Long, val title: String)

    data class AudioSource(val urls: List<String>, val mimeType: String)

    data class SearchHit(
        val keyword: String,
        val bvid: String,
        val link: String,
        val title: String,
        val play: Long
    )

    @Volatile
    var cookie: String = ""
        private set

    @Volatile
    var imgKey: String = ""
        private set

    @Volatile
    var subKey: String = ""
        private set

    @Volatile
    var initialized: Boolean = false
        private set

    fun initialize() {
        val anonymous = fetchAnonymousCookie()
        cookie = anonymous.ifEmpty { BiliUtils.FALLBACK_COOKIE }
        val keys = fetchWbiKeys()
        imgKey = keys.first.ifEmpty { BiliUtils.FALLBACK_IMG_KEY }
        subKey = keys.second.ifEmpty { BiliUtils.FALLBACK_SUB_KEY }
        initialized = true
    }

    private fun fetchAnonymousCookie(): String {
        val resp = http.get(SPI_URL, mapOf("Referer" to "https://www.bilibili.com/"))
        if (!resp.success || resp.body.isEmpty()) return ""
        val data = jsonObject(resp.body)?.get("data") as? JsonObject ?: return ""
        val buvid3 = stringOf(data["b_3"]) ?: ""
        val buvid4 = stringOf(data["b_4"]) ?: ""
        val parts = ArrayList<String>(2)
        if (buvid3.isNotEmpty()) parts.add("buvid3=$buvid3")
        if (buvid4.isNotEmpty()) parts.add("buvid4=$buvid4")
        return parts.joinToString("; ")
    }

    /** `nav` 未登录返回 `code=-101` 但仍带 `data.wbi_img`，因此只判字段存在与否。 */
    private fun fetchWbiKeys(): Pair<String, String> {
        val resp = http.get(
            NAV_URL,
            mapOf("Referer" to "https://www.bilibili.com/", "Cookie" to cookie)
        )
        if (!resp.success || resp.body.isEmpty()) return Pair("", "")
        val data = jsonObject(resp.body)?.get("data") as? JsonObject ?: return Pair("", "")
        val wbiImg = data["wbi_img"] as? JsonObject ?: return Pair("", "")
        val img = extractWbiKey(stringOf(wbiImg["img_url"]) ?: "")
        val sub = extractWbiKey(stringOf(wbiImg["sub_url"]) ?: "")
        return Pair(img, sub)
    }

    private fun extractWbiKey(url: String): String {
        val slash = url.lastIndexOf('/')
        val dot = url.lastIndexOf('.')
        if (slash < 0 || dot <= slash) return ""
        return url.substring(slash + 1, dot)
    }

    suspend fun fetchVideoInfo(bvid: String): AidCid? = withContext(Dispatchers.IO) {
        fetchVideoInfoBy("bvid", bvid)
    }

    suspend fun fetchVideoInfoByAid(aid: Long): AidCid? = withContext(Dispatchers.IO) {
        fetchVideoInfoBy("aid", aid.toString())
    }

    private fun fetchVideoInfoBy(param: String, value: String): AidCid? {
        val resp = http.get(
            "$VIEW_URL?$param=$value",
            mapOf("Referer" to BiliUtils.REFERER, "Origin" to BiliUtils.REFERER)
        )
        if (!resp.success) return null
        val data = jsonObject(resp.body)?.get("data") as? JsonObject ?: return null
        val aid = (data["aid"] as? JsonPrimitive)?.longOrNull ?: return null
        val cid = (data["cid"] as? JsonPrimitive)?.longOrNull ?: return null
        val title = stringOf(data["title"]) ?: ""
        return AidCid(aid, cid, title)
    }

    suspend fun getAudioUrl(aid: Long, cid: Long): AudioSource? = withContext(Dispatchers.IO) {
        val url = "$PLAYURL_URL?avid=$aid&cid=$cid&qn=0&type=&otype=json&fnver=0&fnval=80"
        val resp = http.get(
            url,
            mapOf("Referer" to BiliUtils.REFERER, "Origin" to BiliUtils.REFERER)
        )
        if (!resp.success) return@withContext null
        val root = jsonObject(resp.body) ?: return@withContext null
        val stream = findAudioStream(root) ?: return@withContext null
        val base = stringOf(stream["baseUrl"]) ?: stringOf(stream["base_url"]) ?: ""
        if (base.isEmpty()) return@withContext null
        val urls = ArrayList<String>()
        urls.add(base)
        val backups = stream["backupUrl"] as? JsonArray ?: stream["backup_url"] as? JsonArray
        if (backups != null) {
            for (item in backups) {
                val u = stringOf(item) ?: continue
                if (u.isNotEmpty() && !urls.contains(u)) urls.add(u)
            }
        }
        val mime = stringOf(stream["mimeType"]) ?: stringOf(stream["mime_type"]) ?: "audio/mp4"
        AudioSource(urls, mime)
    }

    /** 递归查找 `dash.audio[i].baseUrl`，语义同 C++ 的 `findUrl`。 */
    private fun findAudioStream(element: JsonElement): JsonObject? {
        when (element) {
            is JsonObject -> {
                val dash = element["dash"] as? JsonObject
                val audio = dash?.get("audio") as? JsonArray
                if (audio != null && audio.isNotEmpty()) {
                    for (item in audio) {
                        val obj = item as? JsonObject ?: continue
                        if (obj["baseUrl"] != null || obj["base_url"] != null) return obj
                    }
                }
                for (value in element.values) {
                    if (value is JsonObject || value is JsonArray) {
                        val found = findAudioStream(value)
                        if (found != null) return found
                    }
                }
            }
            is JsonArray -> {
                for (item in element) {
                    val found = findAudioStream(item)
                    if (found != null) return found
                }
            }
            else -> Unit
        }
        return null
    }

    suspend fun searchSingle(keyword: String): SearchHit? = withContext(Dispatchers.IO) {
        if (!initialized) initialize()
        val params = mapOf("keyword" to keyword, "page" to "1", "pagesize" to "20")
        val signed = BiliUtils.signWbi(params, imgKey, subKey, System.currentTimeMillis() / 1000)
        val url = StringBuilder(SEARCH_URL).append('?')
        var first = true
        for ((k, v) in signed) {
            if (!first) url.append('&')
            first = false
            url.append(k).append('=').append(BiliUtils.urlEncodeWbi(v))
        }
        val resp = http.get(
            url.toString(),
            mapOf("Referer" to BiliUtils.REFERER, "Cookie" to cookie)
        )
        if (!resp.success || resp.body.isEmpty()) return@withContext null
        val root = jsonObject(resp.body) ?: return@withContext null
        val code = (root["code"] as? JsonPrimitive)?.longOrNull ?: -1L
        if (code != 0L) return@withContext null
        val data = root["data"] as? JsonObject ?: return@withContext null
        val groups = data["result"] as? JsonArray ?: return@withContext null

        val candidates = ArrayList<SearchHit>()
        for (group in groups) {
            val groupObj = group as? JsonObject ?: continue
            if (stringOf(groupObj["result_type"]) != "video") continue
            val videos = groupObj["data"] as? JsonArray ?: continue
            for (video in videos) {
                val obj = video as? JsonObject ?: continue
                val durationElement = obj["duration"] ?: continue
                val duration = BiliUtils.parseDurationToSeconds(durationElement)
                if (duration <= 0 || duration > MAX_DURATION_SECONDS) continue
                val bvid = stringOf(obj["bvid"]) ?: continue
                if (bvid.isEmpty()) continue
                val play = (obj["play"] as? JsonPrimitive)?.longOrNull
                    ?: stringOf(obj["play"])?.toLongOrNull() ?: 0L
                val title = BiliUtils.stripHtmlTags(stringOf(obj["title"]) ?: "")
                candidates.add(
                    SearchHit(keyword, bvid, "https://www.bilibili.com/video/$bvid", title, play)
                )
            }
        }
        if (candidates.isEmpty()) return@withContext null
        candidates.sortByDescending { it.play }
        candidates.first()
    }

    /** 逗号分隔多关键词；优化词失败时降级原词。 */
    suspend fun search(keywordsCsv: String): List<SearchHit> = withContext(Dispatchers.IO) {
        val results = ArrayList<SearchHit>()
        for (raw in keywordsCsv.split(",")) {
            val kw = raw.trim()
            if (kw.isEmpty()) continue
            val optimized = BiliUtils.optimizeKeyword(kw)
            var hit = searchSingle(optimized)
            if (hit == null && optimized != kw) hit = searchSingle(kw)
            if (hit != null) results.add(hit)
        }
        results
    }

    fun resolveUrl(input: String): String {
        val url = BiliUtils.resolveUrlInput(input) ?: return ""
        if (!url.contains("b23.tv")) return url
        return http.resolveRedirect(url) ?: url
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
        private const val SPI_URL = "https://api.bilibili.com/x/frontend/finger/spi"
        private const val NAV_URL = "https://api.bilibili.com/x/web-interface/nav"
        private const val VIEW_URL = "https://api.bilibili.com/x/web-interface/view"
        private const val PLAYURL_URL = "https://api.bilibili.com/x/player/playurl"
        private const val SEARCH_URL = "https://api.bilibili.com/x/web-interface/wbi/search/all/v2"
        private const val MAX_DURATION_SECONDS = 600
    }
}
