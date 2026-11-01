package com.narnat.music.core.lyrics

import java.io.ByteArrayOutputStream
import java.util.Base64
import java.util.Locale
import java.util.zip.DataFormatException
import java.util.zip.Inflater
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive

/**
 * 酷狗 KRC 解密 + 汽水 KRC/JSON 转 LRC。逐条对齐 `kugou_provider.cpp` 与 `qishui_provider.cpp`。
 */
object KrcDecoder {

    private val XOR_KEY = byteArrayOf(
        64, 71, 97, 119, 94, 50, 116, 71, 81, 54, 49, 45, 206.toByte(), 210.toByte(), 110, 105
    )

    private val KRC_TIMESTAMP = Regex("""\[(\d+),\d+]""")
    private val KRC_LINE = Regex("""\[(\d+),(\d+)](.*)""")
    private val WORD_TAG = Regex("""<[^>]*>""")

    /**
     * 校验 `krc1` 魔数 → 16 字节 XOR 解密 → zlib 解压 → 拆分 `[language:` 段。
     * @return 第一项为正文 LRC，第二项为译文 LRC（可能为空）；失败返回 null。
     */
    fun decryptKrc(data: ByteArray): Pair<String, String>? {
        if (data.size < 8) return null
        if (data[0] != 'k'.code.toByte() || data[1] != 'r'.code.toByte() ||
            data[2] != 'c'.code.toByte() || data[3] != '1'.code.toByte()
        ) {
            return null
        }
        val decrypted = ByteArray(data.size - 4)
        for (i in 4 until data.size) {
            decrypted[i - 4] = (data[i].toInt() xor XOR_KEY[(i - 4) % 16].toInt()).toByte()
        }
        val decompressed = inflate(decrypted) ?: return null
        val fullText = String(decompressed, Charsets.UTF_8)

        val timestamps = ArrayList<Long>()
        for (m in KRC_TIMESTAMP.findAll(fullText)) {
            timestamps.add(m.groupValues[1].toLongOrNull() ?: 0L)
        }

        val langPos = fullText.indexOf("[language:")
        if (langPos < 0) return Pair(fullText, "")

        val outLyrics = fullText.substring(0, langPos)
        val b64Start = langPos + "[language:".length
        val b64End = fullText.indexOf(']', b64Start)
        if (b64End <= b64Start) return Pair(outLyrics, "")

        val langJson = decodeBase64(fullText.substring(b64Start, b64End))
        if (langJson.isEmpty()) return Pair(outLyrics, "")

        val root = try {
            Json.parseToJsonElement(langJson)
        } catch (e: IllegalArgumentException) {
            return Pair(outLyrics, "")
        } catch (e: kotlinx.serialization.SerializationException) {
            return Pair(outLyrics, "")
        }
        val contentArray = (root as? JsonObject)?.get("content") as? JsonArray
            ?: return Pair(outLyrics, "")

        var bestIdx = -1
        var bestRatio = 0.0
        val allTransLines = ArrayList<List<String>>()
        for (item in contentArray) {
            val obj = item as? JsonObject ?: continue
            val lyricContent = obj["lyricContent"] as? JsonArray ?: continue
            val transLines = ArrayList<String>()
            var totalAlpha = 0
            var chineseCount = 0
            for (lineElement in lyricContent) {
                val lineArray = lineElement as? JsonArray ?: continue
                val sb = StringBuilder()
                for (part in lineArray) {
                    sb.append((part as? JsonPrimitive)?.content ?: "")
                }
                val line = sb.toString()
                val bytes = line.toByteArray(Charsets.UTF_8)
                for (b in bytes) {
                    val c = b.toInt() and 0xFF
                    if ((c and 0x80) == 0) continue
                    if (c >= 0xE4 && c <= 0xE9) {
                        chineseCount++
                        totalAlpha++
                    }
                }
                totalAlpha += bytes.size
                transLines.add(line)
            }
            val ratio = if (totalAlpha > 0) chineseCount.toDouble() / totalAlpha else 0.0
            allTransLines.add(transLines)
            if (ratio > bestRatio) {
                bestRatio = ratio
                bestIdx = allTransLines.size - 1
            }
        }
        if (bestIdx < 0 || bestRatio <= 0.01) return Pair(outLyrics, "")

        val transLrc = StringBuilder()
        var tsIdx = 0
        for (line in allTransLines[bestIdx]) {
            if (tsIdx >= timestamps.size) break
            val ms = timestamps[tsIdx++]
            if (line.isEmpty() || line == " ") continue
            transLrc.append(formatTimestamp(ms, 2)).append(line).append('\n')
        }
        return Pair(outLyrics, transLrc.toString())
    }

    /** 汽水 `lyric.type == "krc"` 的词级 KRC 文本转 LRC（剔除 `<...>` 标记）。 */
    fun krcStringToLrc(krcContent: String): String {
        val sb = StringBuilder()
        for (rawLine in krcContent.split("\n")) {
            val line = rawLine.replace("\r", "")
            val match = KRC_LINE.matchEntire(line) ?: continue
            val startMs = match.groupValues[1].toLongOrNull() ?: 0L
            val text = WORD_TAG.replace(match.groupValues[3], "")
            if (text.isEmpty()) continue
            sb.append(formatTimestamp(startMs, 3)).append(text).append('\n')
        }
        return sb.toString()
    }

    /** 汽水分享页 `lyrics` 对象（`{"sentences":[...]}`）转 LRC，返回（正文, 译文）。 */
    fun krcJsonToLrc(krcJson: String): Pair<String, String> {
        val root = try {
            Json.parseToJsonElement(krcJson)
        } catch (e: IllegalArgumentException) {
            return Pair("", "")
        } catch (e: kotlinx.serialization.SerializationException) {
            return Pair("", "")
        }
        val sentences = (root as? JsonObject)?.get("sentences") as? JsonArray
            ?: return Pair("", "")

        val lrc = StringBuilder()
        val transLrc = StringBuilder()
        var hasTrans = false
        for (element in sentences) {
            val sentence = element as? JsonObject ?: continue
            var startMs = numberOf(sentence["startMs"]) ?: 0L
            val words = sentence["words"] as? JsonArray
            if (startMs == 0L && words != null && words.isNotEmpty()) {
                val firstWord = words[0] as? JsonObject
                startMs = if (firstWord != null) (numberOf(firstWord["startMs"]) ?: 0L) else 0L
            }

            val textSb = StringBuilder()
            if (words != null && words.isNotEmpty()) {
                for (w in words) {
                    val word = w as? JsonObject ?: continue
                    textSb.append(stringOf(word["text"]) ?: "")
                }
            }
            var text = textSb.toString()
            if (text.isEmpty()) text = stringOf(sentence["text"]) ?: ""
            if (text.isEmpty()) continue

            val ts = formatTimestamp(startMs, 3)
            lrc.append(ts).append(text).append('\n')

            var transText = ""
            val translatedWords = sentence["translatedWords"] as? JsonArray
            if (translatedWords != null && translatedWords.isNotEmpty()) {
                val transSb = StringBuilder()
                for (w in translatedWords) {
                    val word = w as? JsonObject ?: continue
                    transSb.append(stringOf(word["text"]) ?: "")
                }
                transText = transSb.toString()
            } else {
                transText = stringOf(sentence["translation"])
                    ?: stringOf(sentence["trans"])
                    ?: stringOf(sentence["translatedText"])
                    ?: ""
            }
            if (transText.isNotEmpty()) {
                hasTrans = true
                transLrc.append(ts).append(transText).append('\n')
            }
        }
        return Pair(lrc.toString(), if (hasTrans) transLrc.toString() else "")
    }

    private fun numberOf(element: JsonElement?): Long? {
        val primitive = element as? JsonPrimitive ?: return null
        if (primitive.isString) return null
        return primitive.content.toLongOrNull()
    }

    private fun stringOf(element: JsonElement?): String? {
        val primitive = element as? JsonPrimitive ?: return null
        if (!primitive.isString) return null
        return primitive.content
    }

    private fun formatTimestamp(ms: Long, fractionDigits: Int): String {
        val min = ms / 60000
        val sec = (ms % 60000) / 1000
        val frac = ms % 1000
        return if (fractionDigits == 3) {
            String.format(Locale.ROOT, "[%02d:%02d.%03d]", min, sec, frac)
        } else {
            String.format(Locale.ROOT, "[%02d:%02d.%02d]", min, sec, frac / 10)
        }
    }

    private fun decodeBase64(text: String): String {
        if (text.isEmpty()) return ""
        return try {
            String(Base64.getMimeDecoder().decode(text), Charsets.UTF_8)
        } catch (e: IllegalArgumentException) {
            ""
        }
    }

    private fun inflate(data: ByteArray): ByteArray? {
        val inflater = Inflater()
        inflater.setInput(data)
        val out = ByteArrayOutputStream()
        val buf = ByteArray(8192)
        try {
            while (!inflater.finished()) {
                val n = inflater.inflate(buf)
                if (n == 0) {
                    if (inflater.needsInput() || inflater.needsDictionary()) break
                }
                out.write(buf, 0, n)
            }
        } catch (e: DataFormatException) {
            return null
        } finally {
            inflater.end()
        }
        return out.toByteArray()
    }
}
