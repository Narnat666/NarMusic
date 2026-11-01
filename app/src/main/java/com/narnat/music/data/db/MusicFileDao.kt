package com.narnat.music.data.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.Query
import androidx.room.Update
import kotlinx.coroutines.flow.Flow

@Dao
interface MusicFileDao {

    @Query("SELECT * FROM music_files ORDER BY downloaded_at DESC")
    fun observeAll(): Flow<List<MusicFileEntity>>

    @Query("SELECT * FROM music_files WHERE id = :id")
    suspend fun findById(id: Long): MusicFileEntity?

    @Query("SELECT * FROM music_files ORDER BY downloaded_at DESC")
    suspend fun findAll(): List<MusicFileEntity>

    @Query("SELECT * FROM music_files WHERE system_filename = :filename")
    suspend fun findBySystemFilename(filename: String): MusicFileEntity?

    @Query(
        "SELECT * FROM music_files WHERE song_name = :songName AND artist = :artist " +
            "AND platform = :platform AND delay_ms = :delayMs " +
            "ORDER BY downloaded_at DESC LIMIT 1"
    )
    suspend fun findBySongArtistPlatformDelay(
        songName: String,
        artist: String,
        platform: String,
        delayMs: Int
    ): MusicFileEntity?

    @Insert
    suspend fun insert(entity: MusicFileEntity): Long

    @Update
    suspend fun update(entity: MusicFileEntity)

    @Query("DELETE FROM music_files WHERE id = :id")
    suspend fun deleteById(id: Long)

    @Query("UPDATE music_files SET in_use = 1, last_used_at = :nowSec WHERE id = :id")
    suspend fun markUsed(id: Long, nowSec: Long)

    @Query("SELECT COUNT(*) FROM music_files WHERE file_path = :filePath")
    suspend fun countByFilePath(filePath: String): Int

    @Query("SELECT * FROM music_files WHERE last_used_at < :thresholdSec AND in_use = 0")
    suspend fun findExpired(thresholdSec: Long): List<MusicFileEntity>
}
