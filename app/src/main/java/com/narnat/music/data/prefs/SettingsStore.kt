package com.narnat.music.data.prefs

import android.content.Context
import androidx.core.content.edit
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * 轻量设置存储（SharedPreferences，避免额外引入 DataStore）。
 */
class SettingsStore(context: Context) {

    private val prefs = context.applicationContext
        .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    private val _lyricsPlatform = MutableStateFlow(
        prefs.getString(KEY_LYRICS_PLATFORM, PLATFORM_KUGOU) ?: PLATFORM_KUGOU
    )
    val lyricsPlatform: StateFlow<String> = _lyricsPlatform.asStateFlow()

    private val _delayMs = MutableStateFlow(prefs.getInt(KEY_DELAY_MS, 0))
    val delayMs: StateFlow<Int> = _delayMs.asStateFlow()

    private val _darkMode = MutableStateFlow(
        prefs.getString(KEY_DARK_MODE, DarkMode.Follow.name) ?: DarkMode.Follow.name
    )
    val darkMode: StateFlow<String> = _darkMode.asStateFlow()

    private val _autoPlayAfterDownload = MutableStateFlow(prefs.getBoolean(KEY_AUTO_PLAY, false))
    val autoPlayAfterDownload: StateFlow<Boolean> = _autoPlayAfterDownload.asStateFlow()

    fun setLyricsPlatform(platform: String) {
        prefs.edit { putString(KEY_LYRICS_PLATFORM, platform) }
        _lyricsPlatform.value = platform
    }

    fun setDelayMs(delayMs: Int) {
        prefs.edit { putInt(KEY_DELAY_MS, delayMs) }
        _delayMs.value = delayMs
    }

    fun setDarkMode(mode: DarkMode) {
        prefs.edit { putString(KEY_DARK_MODE, mode.name) }
        _darkMode.value = mode.name
    }

    fun setAutoPlayAfterDownload(enabled: Boolean) {
        prefs.edit { putBoolean(KEY_AUTO_PLAY, enabled) }
        _autoPlayAfterDownload.value = enabled
    }

    enum class DarkMode { Follow, On, Off }

    companion object {
        const val PLATFORM_KUGOU = "酷狗音乐"

        private const val PREFS_NAME = "narmusic_settings"
        private const val KEY_LYRICS_PLATFORM = "lyricsPlatform"
        private const val KEY_DELAY_MS = "delayMs"
        private const val KEY_DARK_MODE = "darkMode"
        private const val KEY_AUTO_PLAY = "autoPlayAfterDownload"
    }
}
