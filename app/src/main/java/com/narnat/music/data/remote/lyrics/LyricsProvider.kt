package com.narnat.music.data.remote.lyrics

import java.net.URLEncoder

/**
 * 单个平台的歌词抓取契约。实现必须自行处理网络/解析异常，失败时返回 `hasLyrics=false` 的结果。
 */
interface LyricsProvider {
    val name: String

    suspend fun fetch(keyword: String): PlatformResult
}

data class PlatformResult(
    val source: String,
    val songId: String = "",
    val songName: String = "",
    val artist: String = "",
    val album: String = "",
    val lyrics: String = "",
    val translation: String = "",
    val cover: ByteArray? = null,
    val hasLyrics: Boolean = false,
    val hasTranslation: Boolean = false,
    val hasCover: Boolean = false,
    val hasAlbum: Boolean = false
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is PlatformResult) return false
        if (source != other.source) return false
        if (songId != other.songId) return false
        if (songName != other.songName) return false
        if (artist != other.artist) return false
        if (album != other.album) return false
        if (lyrics != other.lyrics) return false
        if (translation != other.translation) return false
        if (hasLyrics != other.hasLyrics) return false
        if (hasTranslation != other.hasTranslation) return false
        if (hasCover != other.hasCover) return false
        if (hasAlbum != other.hasAlbum) return false
        val a = cover
        val b = other.cover
        if (a == null) return b == null
        return b != null && a.contentEquals(b)
    }

    override fun hashCode(): Int {
        var result = source.hashCode()
        result = 31 * result + songId.hashCode()
        result = 31 * result + songName.hashCode()
        result = 31 * result + artist.hashCode()
        result = 31 * result + album.hashCode()
        result = 31 * result + lyrics.hashCode()
        result = 31 * result + translation.hashCode()
        result = 31 * result + (cover?.contentHashCode() ?: 0)
        result = 31 * result + hasLyrics.hashCode()
        result = 31 * result + hasTranslation.hashCode()
        result = 31 * result + hasCover.hashCode()
        result = 31 * result + hasAlbum.hashCode()
        return result
    }
}

/** 封面不足 5KB 视为无效（与 C++ 各 Provider 的门槛一致）。 */
internal const val MIN_COVER_BYTES = 5120

internal const val PLATFORM_KUGOU = "酷狗音乐"
internal const val PLATFORM_NETEASE = "网易云音乐"
internal const val PLATFORM_QQ = "QQ音乐"
internal const val PLATFORM_QISHUI = "汽水音乐"

/**
 * 查询串转义。`URLEncoder.encode(String, String)` 在 API 33 已废弃但 minSdk 26 必须使用它，
 * 不能用 `encode(String, Charset)`（API 33 起才有）。
 */
@Suppress("DEPRECATION")
internal fun encodeQuery(text: String): String =
    URLEncoder.encode(text, "UTF-8").replace("+", "%20")

/** Provider 内部可变状态，避免多个 out 参数。 */
internal class LyricsDraft {
    var songId: String = ""
    var songName: String = ""
    var artist: String = ""
    var album: String = ""
    var coverUrl: String = ""
    var lyrics: String = ""
    var translation: String = ""
    var hasLyrics: Boolean = false
    var hasTranslation: Boolean = false
    var hasAlbum: Boolean = false

    fun toResult(source: String, cover: ByteArray?): PlatformResult = PlatformResult(
        source = source,
        songId = songId,
        songName = songName,
        artist = artist,
        album = album,
        lyrics = lyrics,
        translation = translation,
        cover = cover,
        hasLyrics = hasLyrics && lyrics.isNotEmpty(),
        hasTranslation = hasTranslation && translation.isNotEmpty(),
        hasCover = cover != null && cover.size >= MIN_COVER_BYTES,
        hasAlbum = hasAlbum || album.isNotEmpty()
    )
}
