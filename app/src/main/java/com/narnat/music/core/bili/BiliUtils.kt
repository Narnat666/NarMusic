package com.narnat.music.core.bili

import java.security.MessageDigest
import java.util.TreeMap
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonPrimitive

/**
 * B 站相关的纯逻辑工具：WBI 签名、链接解析、关键词清洗、时长解析。
 */
object BiliUtils {

    val MIXIN_KEY_ENC_TAB = intArrayOf(
        46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35,
        27, 43, 5, 49, 33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13
    )

    const val FALLBACK_COOKIE =
        "buvid3=175-9E59-4C12-8A3A-3C76C5B6850865482infoc; " +
            "buvid4=6A5E6D2E-1C78-1C42-8F6A-3D7F9C0E5B3265482;"

    const val FALLBACK_IMG_KEY = "7cd084941338484aae1ad9425b84077c"
    const val FALLBACK_SUB_KEY = "4932caff0ff746eab6f01bf08b70ac45"

    const val REFERER = "https://www.bilibili.com"

    private val HEX_UPPER = "0123456789ABCDEF".toCharArray()
    private val HEX_LOWER = "0123456789abcdef".toCharArray()
    private val HTML_TAG = Regex("<[^>]*>")
    private val AV_ID = Regex("/av(\\d+)", RegexOption.IGNORE_CASE)
    private const val TRAILING_PUNCT = "，。、；：！？）》」』】,.、;:!?)]}\"'"

    fun md5Hex(bytes: ByteArray): String {
        val digest = MessageDigest.getInstance("MD5").digest(bytes)
        val sb = StringBuilder(digest.size * 2)
        for (b in digest) {
            val v = b.toInt() and 0xFF
            sb.append(HEX_LOWER[v ushr 4]).append(HEX_LOWER[v and 0x0F])
        }
        return sb.toString()
    }

    fun md5Hex(text: String): String = md5Hex(text.toByteArray(Charsets.UTF_8))

    /** `imgKey + subKey` 按 [MIXIN_KEY_ENC_TAB] 重排取前 32 位。 */
    fun mixinKey(imgKey: String, subKey: String): String {
        val raw = imgKey + subKey
        val sb = StringBuilder(32)
        for (i in 0 until 32) {
            val index = MIXIN_KEY_ENC_TAB[i]
            if (index >= raw.length) return ""
            sb.append(raw[index])
        }
        return sb.toString()
    }

    /**
     * 计算 `w_rid`。参数按 key 字典序拼 query（空值跳过、value 严格 urlEncode），
     * `w_rid = md5(query + mixinKey)`。
     */
    fun signWbi(
        params: Map<String, String>,
        imgKey: String,
        subKey: String,
        wts: Long
    ): Map<String, String> {
        val signed = TreeMap<String, String>(params)
        signed["wts"] = wts.toString()

        val qs = StringBuilder()
        for ((k, v) in signed) {
            if (v.isEmpty()) continue
            if (qs.isNotEmpty()) qs.append('&')
            qs.append(k).append('=').append(urlEncodeWbi(v))
        }
        val result = TreeMap<String, String>(signed)
        result["w_rid"] = md5Hex(qs.toString() + mixinKey(imgKey, subKey))
        return result
    }

    /** 仅 `A-Za-z0-9+-._~` 不转义，其余 `%XX`（大写十六进制）。 */
    fun urlEncodeWbi(text: String): String {
        val sb = StringBuilder()
        for (b in text.toByteArray(Charsets.UTF_8)) {
            val c = b.toInt() and 0xFF
            val unreserved = (c in 'a'.code..'z'.code) || (c in 'A'.code..'Z'.code) ||
                (c in '0'.code..'9'.code) || c == '-'.code || c == '_'.code ||
                c == '.'.code || c == '~'.code
            if (unreserved) {
                sb.append(c.toChar())
            } else {
                sb.append('%').append(HEX_UPPER[c ushr 4]).append(HEX_UPPER[c and 0x0F])
            }
        }
        return sb.toString()
    }

    fun extractBvid(url: String): String? {
        val pos = url.indexOf("BV")
        if (pos < 0 || pos + 12 > url.length) return null
        return url.substring(pos, pos + 12)
    }

    /** `https://www.bilibili.com/video/av123456` 这类老式链接。 */
    fun extractAvId(url: String): Long? {
        val match = AV_ID.find(url) ?: return null
        return match.groupValues[1].toLongOrNull()
    }

    /** 从任意文本里抠出第一个 http 链接（遇到空白、中文或尾随标点即截断）。 */
    fun resolveUrlInput(text: String): String? {
        val start = text.indexOf("http")
        if (start < 0) return null
        var end = start
        while (end < text.length) {
            val c = text[end]
            if (c.code > 0x7F || c == ' ' || c == '\t' || c == '\n' || c == '\r') break
            end++
        }
        if (end <= start) return null
        var url = text.substring(start, end)
        while (url.isNotEmpty() && url.last() in TRAILING_PUNCT) url = url.dropLast(1)
        return if (url.length > "http".length) url else null
    }

    fun optimizeKeyword(raw: String): String {
        val sb = StringBuilder(raw.length)
        for (c in raw) {
            when (c) {
                ' ', '\t', '-', '_', '\u00B7', '\u2014' -> Unit
                else -> sb.append(c)
            }
        }
        return if (sb.isEmpty()) raw else sb.toString()
    }

    fun stripHtmlTags(html: String): String = HTML_TAG.replace(html, "")

    /** 解析 `"3:45"` / `"1:02:03"` / `"419:2"` / `"223"` 这类时长文本。 */
    fun parseDurationToSeconds(raw: String): Int {
        if (raw.isEmpty()) return 0
        if (raw.all { it.isDigit() }) return raw.toIntOrNull() ?: 0
        val parts = ArrayList<Int>()
        val buf = StringBuilder()
        for (c in raw) {
            if (c.isDigit()) {
                buf.append(c)
            } else if (buf.isNotEmpty()) {
                parts.add(buf.toString().toIntOrNull() ?: 0)
                buf.clear()
            }
        }
        if (buf.isNotEmpty()) parts.add(buf.toString().toIntOrNull() ?: 0)
        return when (parts.size) {
            1 -> parts[0]
            2 -> parts[0] * 60 + parts[1]
            3 -> parts[0] * 3600 + parts[1] * 60 + parts[2]
            else -> 0
        }
    }

    /** `duration` 可能是数字也可能是 `"m:ss"` / `"h:mm:ss"` 字符串。 */
    fun parseDurationToSeconds(raw: JsonElement): Int {
        val primitive = raw as? JsonPrimitive ?: return 0
        if (primitive.isString) return parseDurationToSeconds(primitive.content)
        return primitive.content.toLongOrNull()?.toInt()
            ?: primitive.content.toDoubleOrNull()?.toInt()
            ?: 0
    }
}
