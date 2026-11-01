package com.narnat.music.ui

import com.narnat.music.ui.components.formatBytes
import com.narnat.music.ui.components.formatElapsed
import com.narnat.music.ui.components.formatSpeed
import org.junit.Assert.assertEquals
import org.junit.Test

class ByteFormatTest {

    @Test
    fun formatsZero() {
        assertEquals("0 B", formatBytes(0L))
    }

    @Test
    fun formatsBelowOneKilobyte() {
        assertEquals("1023 B", formatBytes(1023L))
        assertEquals("1 B", formatBytes(1L))
    }

    @Test
    fun formatsKilobytes() {
        assertEquals("1.0 KB", formatBytes(1024L))
        assertEquals("1.5 KB", formatBytes(1536L))
    }

    @Test
    fun formatsMegabytes() {
        assertEquals("1.0 MB", formatBytes(1048576L))
        assertEquals("3.2 MB", formatBytes(3355443L))
    }

    @Test
    fun formatsGigabytes() {
        assertEquals("1.0 GB", formatBytes(1073741824L))
    }

    @Test
    fun appendsPerSecondSuffix() {
        assertEquals("0 B/s", formatSpeed(0L))
        assertEquals("1.0 KB/s", formatSpeed(1024L))
    }

    @Test
    fun formatsElapsedDurations() {
        assertEquals("-", formatElapsed(0L))
        assertEquals("1.5秒", formatElapsed(1500L))
        assertEquals("1分5秒", formatElapsed(65000L))
        assertEquals("1小时1分", formatElapsed(3660000L))
    }
}
