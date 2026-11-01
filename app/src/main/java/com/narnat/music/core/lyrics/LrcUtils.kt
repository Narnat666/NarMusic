package com.narnat.music.core.lyrics

import java.util.Locale
import java.util.TreeMap

/** 一行带时间戳的歌词。 */
data class LrcLine(val timeMs: Long, val text: String)

/**
 * LRC 文本处理工具，逐条对齐 NarMusic 的 `infrastructure/lyrics/lyrics_aggregator.cpp`。
 */
object LrcUtils {

    private val TIME_TAG = Regex("""\[(\d+):(\d+)\.(\d+)]""")
    private val EMPTY_LINE = Regex("""\[\d+:\d+\.\d+]\s*\n""")
    private val STANDARDIZE = Regex("""\[(\d+):(\d+)[.:](\d+)]""")
    private val MERGE_LINE = Regex("""^\[(\d{2}):(\d{2})\.(\d{2,3})](.*)$""")

    /**
     * CJK 占比 >= 30% 且日语假名占比 < 10% 判为中文歌词。
     * 逐行照抄 C++（含「累计 10 个有效字符提前返回」「样本不足 3 个直接 false」两处细节）。
     */
    fun isChineseLyrics(text: String): Boolean {
        var cjkCount = 0
        var alphaCount = 0
        var japaneseCount = 0
        var i = 0
        while (i < text.length) {
            if (text[i] == '[') {
                while (i < text.length && text[i] != ']') i++
                if (i < text.length && text[i] == ']') i++
                continue
            }
            val cp = text.codePointAt(i)
            i += Character.charCount(cp)
            if (cp < 0x80) {
                if (isAsciiAlpha(cp)) alphaCount++
            } else if (cp in 0x3040..0x309F || cp in 0x30A0..0x30FF) {
                japaneseCount++
                alphaCount++
            } else if (cp in 0x4E00..0x9FFF || cp in 0x3400..0x4DBF ||
                cp in 0xF900..0xFAFF || cp in 0x20000..0x2A6DF
            ) {
                cjkCount++
                alphaCount++
            }
            if (alphaCount >= 10 && japaneseCount * 10 >= alphaCount) return false
            if (alphaCount >= 10 && cjkCount * 10 >= alphaCount * 3) return true
        }
        if (alphaCount >= 3 && japaneseCount * 10 >= alphaCount) return false
        return alphaCount >= 3 && cjkCount * 10 >= alphaCount * 3
    }

    private fun isAsciiAlpha(cp: Int): Boolean =
        (cp >= 'a'.code && cp <= 'z'.code) || (cp >= 'A'.code && cp <= 'Z'.code)

    /** 移除「时间标签但内容为空」的行。 */
    fun cleanLyrics(text: String): String = EMPTY_LINE.replace(text, "")

    /** 所有 `[mm:ss.xx]` 时间戳整体平移 offsetMs，负数夹到 0。 */
    fun adjustLyricsTiming(lyrics: String, offsetMs: Int): String {
        if (offsetMs == 0 || lyrics.isEmpty()) return lyrics
        return TIME_TAG.replace(lyrics) { m ->
            val min = m.groupValues[1].toInt()
            val sec = m.groupValues[2].toInt()
            val msStr = m.groupValues[3]
            var ms = msStr.toInt()
            if (msStr.length == 2) ms *= 10
            var total = (min * 60L + sec) * 1000L + ms + offsetMs
            if (total < 0) total = 0
            val newMin = total / 60000
            val newSec = (total % 60000) / 1000
            val newMs = total % 1000
            String.format(Locale.ROOT, "[%02d:%02d.%02d]", newMin, newSec, newMs / 10)
        }
    }

    /**
     * 双时间戳合并（椒盐音乐标准）：保留原文的非时间轴元信息行，同一时间戳先原文后译文。
     */
    fun mergeBilingualLyrics(original: String, translation: String): String {
        if (translation.isEmpty()) return original
        val merged = TreeMap<Long, Pair<String, String>>()
        val metaData = ArrayList<String>()

        fun parse(text: String, isTranslation: Boolean) {
            for (raw in text.split("\n")) {
                val line = raw.replace("\r", "")
                val match = MERGE_LINE.matchEntire(line)
                if (match != null) {
                    val mm = match.groupValues[1].toInt()
                    val ss = match.groupValues[2].toInt()
                    val msStr = match.groupValues[3]
                    var ms = msStr.toInt()
                    if (msStr.length == 2) ms *= 10
                    val timestamp = (mm * 60000 + ss * 1000 + ms).toLong()
                    val content = trimSpacesAndTabs(match.groupValues[4])
                    val current = merged[timestamp] ?: Pair("", "")
                    merged[timestamp] = if (isTranslation) {
                        Pair(current.first, content)
                    } else {
                        Pair(content, current.second)
                    }
                } else if (!isTranslation && line.isNotEmpty() && line[0] == '[') {
                    metaData.add(line)
                }
            }
        }

        parse(original, false)
        parse(translation, true)

        val sb = StringBuilder()
        for (meta in metaData) sb.append(meta).append('\n')
        for ((ts, pair) in merged) {
            val mm = ts / 60000
            val ss = (ts % 60000) / 1000
            val xx = (ts % 1000) / 10
            val tsStr = String.format(Locale.ROOT, "[%02d:%02d.%02d]", mm, ss, xx)
            if (pair.first.isNotEmpty()) sb.append(tsStr).append(pair.first).append('\n')
            if (pair.second.isNotEmpty() && pair.second != pair.first) {
                sb.append(tsStr).append(pair.second).append('\n')
            }
        }
        return sb.toString()
    }

    private fun trimSpacesAndTabs(text: String): String {
        var start = 0
        var end = text.length
        while (start < end && (text[start] == ' ' || text[start] == '\t')) start++
        while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t')) end--
        return text.substring(start, end)
    }

    /** 把 `[mm:ss:xx]` 之类的变体统一成 `[mm:ss.xx]`。 */
    fun convertToStandardFormat(text: String): String =
        STANDARDIZE.replace(text) { m -> "[${m.groupValues[1]}:${m.groupValues[2]}.${m.groupValues[3]}]" }

    /**
     * 分词后取多重集交集的占比。C++ 用单字节 `isalnum`，UTF-8 中文会被整段丢弃，
     * 这里改用 Unicode 字母/数字判定，避免中文歌名相似度恒为 0。
     */
    fun nameSimilarity(a: String, b: String): Double {
        if (a.isEmpty() || b.isEmpty()) return 0.0
        val wordsA = extractWords(a).sorted()
        val wordsB = extractWords(b).sorted()
        if (wordsA.isEmpty() || wordsB.isEmpty()) return 0.0
        var i = 0
        var j = 0
        var common = 0
        while (i < wordsA.size && j < wordsB.size) {
            val cmp = wordsA[i].compareTo(wordsB[j])
            when {
                cmp == 0 -> {
                    common++
                    i++
                    j++
                }
                cmp < 0 -> i++
                else -> j++
            }
        }
        return common.toDouble() / minOf(wordsA.size, wordsB.size)
    }

    private fun extractWords(text: String): List<String> {
        val words = ArrayList<String>()
        val sb = StringBuilder()
        var i = 0
        while (i < text.length) {
            val cp = text.codePointAt(i)
            i += Character.charCount(cp)
            if (Character.isLetterOrDigit(cp)) {
                sb.appendCodePoint(Character.toLowerCase(cp))
            } else if (sb.isNotEmpty()) {
                words.add(sb.toString())
                sb.clear()
            }
        }
        if (sb.isNotEmpty()) words.add(sb.toString())
        return words
    }

    /** 解析成播放器可用的时间轴行；同一行多个时间戳会展开，顺序保持文档顺序。 */
    fun parseLrc(text: String): List<LrcLine> {
        val result = ArrayList<LrcLine>()
        for (raw in text.split("\n")) {
            val line = raw.replace("\r", "")
            var searchFrom = 0
            val times = ArrayList<Long>()
            while (true) {
                val m = TIME_TAG.find(line, searchFrom) ?: break
                if (m.range.first != searchFrom) break
                times.add(
                    (m.groupValues[1].toLong() * 60000L +
                        m.groupValues[2].toLong() * 1000L +
                        parseFraction(m.groupValues[3]))
                )
                searchFrom = m.range.last + 1
            }
            if (times.isEmpty()) continue
            val content = line.substring(searchFrom).trim()
            for (t in times) result.add(LrcLine(t, content))
        }
        return result
    }

    private fun parseFraction(msStr: String): Long = when {
        msStr.isEmpty() -> 0L
        msStr.length == 1 -> msStr.toLong() * 100L
        msStr.length == 2 -> msStr.toLong() * 10L
        else -> msStr.substring(0, 3).toLong()
    }
}
