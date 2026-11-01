package com.narnat.music.data.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

@Database(entities = [MusicFileEntity::class], version = 1, exportSchema = false)
abstract class MusicDatabase : RoomDatabase() {

    abstract fun musicFileDao(): MusicFileDao

    companion object {
        private const val DB_NAME = "narmusic.db"

        @Volatile
        private var instance: MusicDatabase? = null

        fun get(context: Context): MusicDatabase =
            instance ?: synchronized(this) {
                instance ?: Room.databaseBuilder(
                    context.applicationContext,
                    MusicDatabase::class.java,
                    DB_NAME
                ).build().also { instance = it }
            }
    }
}
