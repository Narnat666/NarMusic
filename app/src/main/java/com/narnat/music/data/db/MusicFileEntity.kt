package com.narnat.music.data.db

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.Index
import androidx.room.PrimaryKey

/**
 * 音乐库表，字段语义与 C++ 版 `music_files` 一致；
 * `media_uri` / `relative_path` / `mime_type` / `duration_ms` 为安卓新增。
 */
@Entity(
    tableName = "music_files",
    indices = [
        Index("downloaded_at"),
        Index("system_filename"),
        Index(value = ["song_name", "artist", "platform", "delay_ms"])
    ]
)
data class MusicFileEntity(
    @PrimaryKey(autoGenerate = true) val id: Long = 0,
    @ColumnInfo(name = "song_name") val songName: String = "",
    val artist: String = "",
    val album: String = "",
    @ColumnInfo(name = "file_path") val filePath: String,
    @ColumnInfo(name = "system_filename") val systemFilename: String,
    @ColumnInfo(name = "original_filename") val originalFilename: String? = null,
    @ColumnInfo(name = "file_size") val fileSize: Long = 0,
    @ColumnInfo(name = "delay_ms") val delayMs: Int = 0,
    val platform: String = "",
    @ColumnInfo(name = "in_use") val inUse: Boolean = false,
    @ColumnInfo(name = "downloaded_at") val downloadedAt: Long,
    @ColumnInfo(name = "last_used_at") val lastUsedAt: Long,
    val lyrics: String? = null,
    @ColumnInfo(name = "media_uri") val mediaUri: String = "",
    @ColumnInfo(name = "relative_path") val relativePath: String = "",
    val mimeType: String = "audio/mp4",
    @ColumnInfo(name = "duration_ms") val durationMs: Long = 0
) {
    /** 文件已丢失（refresh() 把 fileSize 归零标记），供 UI 置灰显示。 */
    val isMissing: Boolean get() = fileSize <= 0L
}
