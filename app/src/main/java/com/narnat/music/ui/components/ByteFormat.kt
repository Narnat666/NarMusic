package com.narnat.music.ui.components

import java.util.Locale

private val UNITS = arrayOf("KB", "MB", "GB", "TB")

fun formatBytes(bytes: Long): String {
    if (bytes < 1024L) return "$bytes B"
    var value = bytes.toDouble() / 1024.0
    var unit = 0
    while (value >= 1024.0 && unit < UNITS.lastIndex) {
        value /= 1024.0
        unit++
    }
    return String.format(Locale.US, "%.1f %s", value, UNITS[unit])
}

fun formatSpeed(bytesPerSecond: Long): String = "${formatBytes(bytesPerSecond)}/s"

fun formatElapsed(elapsedMs: Long): String {
    if (elapsedMs <= 0L) return "-"
    val totalSeconds = elapsedMs / 1000L
    return when {
        totalSeconds < 60L -> String.format(Locale.US, "%.1f秒", elapsedMs / 1000.0)
        totalSeconds < 3600L -> "${totalSeconds / 60L}分${totalSeconds % 60L}秒"
        else -> "${totalSeconds / 3600L}小时${(totalSeconds % 3600L) / 60L}分"
    }
}

fun formatDuration(durationMs: Long): String {
    if (durationMs <= 0L) return "00:00"
    val totalSeconds = durationMs / 1000L
    return String.format(Locale.US, "%02d:%02d", totalSeconds / 60L, totalSeconds % 60L)
}
