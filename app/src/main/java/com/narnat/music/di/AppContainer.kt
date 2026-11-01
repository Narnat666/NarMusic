package com.narnat.music.di

import android.content.Context
import com.narnat.music.data.db.MusicDatabase
import com.narnat.music.data.db.MusicFileDao
import com.narnat.music.data.library.MusicLibraryRepository
import com.narnat.music.data.prefs.SettingsStore
import com.narnat.music.data.remote.HttpClient
import com.narnat.music.data.remote.bili.BiliClient
import com.narnat.music.data.remote.lyrics.KugouProvider
import com.narnat.music.data.remote.lyrics.LyricsAggregator
import com.narnat.music.data.remote.lyrics.LyricsProvider
import com.narnat.music.data.remote.lyrics.NeteaseProvider
import com.narnat.music.data.remote.lyrics.QishuiProvider
import com.narnat.music.data.remote.lyrics.QQMusicProvider
import com.narnat.music.domain.DownloadService
import java.io.File

/**
 * 手写依赖容器，由 `NarMusicApp` 在 `onCreate` 中构建。
 */
class AppContainer(context: Context) {

    private val appContext: Context = context.applicationContext

    val http: HttpClient = HttpClient()

    val biliClient: BiliClient = BiliClient(http)

    val database: MusicDatabase = MusicDatabase.get(appContext)

    val musicFileDao: MusicFileDao = database.musicFileDao()

    val libraryRepository: MusicLibraryRepository = MusicLibraryRepository(appContext, musicFileDao)

    val settingsStore: SettingsStore = SettingsStore(appContext)

    val providers: List<LyricsProvider> = listOf(
        KugouProvider(http),
        NeteaseProvider(http),
        QQMusicProvider(http),
        QishuiProvider(http)
    )

    val lyricsAggregator: LyricsAggregator = LyricsAggregator(
        providers = providers,
        preferredPlatform = settingsStore.lyricsPlatform.value
    )

    val downloadService: DownloadService = DownloadService(
        biliClient = biliClient,
        http = http,
        lyricsAggregator = lyricsAggregator,
        library = libraryRepository,
        dao = musicFileDao,
        cacheDir = File(appContext.cacheDir, CACHE_DIR_NAME)
    )

    fun syncPreferredPlatform() {
        lyricsAggregator.preferredPlatform = settingsStore.lyricsPlatform.value
    }

    companion object {
        private const val CACHE_DIR_NAME = "narmusic"
    }
}
