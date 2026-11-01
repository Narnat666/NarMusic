package com.narnat.music.ui

import androidx.compose.ui.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.narnat.music.ui.theme.immersiveBrush
import com.narnat.music.ui.theme.mix
import com.narnat.music.ui.theme.paletteFromCover
import com.narnat.music.ui.theme.playerBrush
import com.narnat.music.ui.theme.wash
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class NarPaletteTest {

    @Test
    fun nullCoverBytesGivesNullPalette() {
        assertNull(paletteFromCover(null))
    }

    @Test
    fun emptyCoverBytesGivesNullPalette() {
        assertNull(paletteFromCover(ByteArray(0)))
    }

    @Test
    fun invalidCoverBytesGivesNullPalette() {
        assertNull(paletteFromCover(byteArrayOf(1, 2, 3, 4, 5, 6, 7, 8)))
    }

    @Test
    fun mixReturnsEndpointsExactly() {
        val from = Color(0xFF102030)
        val to = Color(0xFFA0B0C0)
        assertEquals(from, mix(from, to, 0f))
        assertEquals(to, mix(from, to, 1f))
    }

    @Test
    fun mixClampsFractionOutOfRange() {
        val from = Color(0xFF000000)
        val to = Color(0xFFFFFFFF)
        assertEquals(from, mix(from, to, -3f))
        assertEquals(to, mix(from, to, 4f))
    }

    @Test
    fun mixAtHalfBlendsChannels() {
        val blended = mix(Color(0xFF000000), Color(0xFFFFFFFF), 0.5f)
        assertEquals(0.5f, blended.red, 0.01f)
        assertEquals(0.5f, blended.green, 0.01f)
        assertEquals(0.5f, blended.blue, 0.01f)
    }

    @Test
    fun washIsMixWithTargetAsBase() {
        val accent = Color(0xFF2D7BC1)
        val base = Color(0xFFFFFFFF)
        assertEquals(mix(base, accent, 0.3f), wash(accent, base, 0.3f))
    }

    @Test
    fun immersiveBrushUsableWithAndWithoutPalette() {
        assertNotNull(immersiveBrush(null, dark = false))
        assertNotNull(immersiveBrush(null, dark = true))
    }

    @Test
    fun playerBrushUsableWithAndWithoutPalette() {
        assertNotNull(playerBrush(null, dark = false))
        assertNotNull(playerBrush(null, dark = true))
    }
}
