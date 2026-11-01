package com.narnat.music.domain

/**
 * 歌单解析结果。`exact = true` 表示 narmeta 精确行（`url|平台|延迟ms|文件名`），不需要再搜索。
 */
data class PlaylistItem(
    val title: String,
    val artist: String,
    val url: String = "",
    val platform: String = "",
    val delayMs: Int = 0,
    val filename: String = "",
    val exact: Boolean = false,
    val original: String = ""
) {
    val keyword: String get() = if (artist.isNotEmpty()) "$title $artist" else title
}

/**
 * 逐行解析歌单文本，语义逐条对齐 `web/app/modules/batch.js` 的 `parsePlaylistText`。
 */
object PlaylistParser {

    private val URL_PATTERN = Regex("^https?://")

    fun parse(text: String): List<PlaylistItem> {
        val items = ArrayList<PlaylistItem>()
        for (raw in text.split("\n")) {
            val line = raw.trim()
            if (line.isEmpty()) continue
            val exact = parseExactLine(line)
            if (exact != null) {
                items.add(exact)
                continue
            }
            val item = parsePlainLine(line)
            if (item != null) items.add(item)
        }
        return items
    }

    private fun parseExactLine(line: String): PlaylistItem? {
        if (!line.contains('|')) return null
        val parts = line.split("|")
        if (parts.size < 4 || !URL_PATTERN.containsMatchIn(parts[0])) return null
        val url = parts[0].trim()
        val platform = parts[1].trim()
        val delayMs = parts[2].trim().toIntOrNull() ?: 0
        val filename = parts.subList(3, parts.size).joinToString("|").trim()
        val title: String
        val artist: String
        val separator = filename.indexOf(" - ")
        if (separator >= 0) {
            title = filename.substring(0, separator).trim()
            artist = filename.substring(separator + 3).trim()
        } else {
            title = filename
            artist = ""
        }
        return PlaylistItem(
            title = title,
            artist = artist,
            url = url,
            platform = platform,
            delayMs = delayMs,
            filename = filename,
            exact = true,
            original = line
        )
    }

    private fun parsePlainLine(line: String): PlaylistItem? {
        val title: String
        val artist: String
        val dash = line.indexOf(" - ")
        val single = line.indexOf('-')
        if (dash >= 0) {
            title = line.substring(0, dash).trim()
            artist = line.substring(dash + 3).trim()
        } else if (single >= 0) {
            title = line.substring(0, single).trim()
            artist = line.substring(single + 1).trim()
        } else {
            title = line
            artist = ""
        }
        if (title.isEmpty()) return null
        return PlaylistItem(title = title, artist = artist, original = line)
    }
}
