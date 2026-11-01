package com.narnat.music

import com.narnat.music.core.lyrics.LrcLine
import com.narnat.music.core.lyrics.LrcUtils
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LrcUtilsTest {

    @Test
    fun detectsChineseAndJapanese() {
        assertTrue(
            LrcUtils.isChineseLyrics(
                "[00:01.00]对这个世界如果你有太多的抱怨\n[00:02.00]跌倒了就不敢继续往前走\n"
            )
        )
        assertFalse(
            LrcUtils.isChineseLyrics(
                "[00:01.00]ありがとう さようなら またね\n[00:02.00]きみのこえがきこえる\n"
            )
        )
        assertFalse(
            LrcUtils.isChineseLyrics(
                "[00:06.22]Hello, it's me\n[00:11.32]I was wondering if after all these years\n"
            )
        )
        assertTrue("无歌词内容时判为否", !LrcUtils.isChineseLyrics(""))
    }

    @Test
    fun adjustsTimingByOffset() {
        assertEquals("[00:02.00]abc", LrcUtils.adjustLyricsTiming("[00:01.00]abc", 1000))
        assertEquals("[00:00.50]abc", LrcUtils.adjustLyricsTiming("[00:01.00]abc", -500))
        assertEquals("[00:00.00]abc", LrcUtils.adjustLyricsTiming("[00:00.10]abc", -500))
        assertEquals("[00:02.50]abc", LrcUtils.adjustLyricsTiming("[00:01.500]abc", 1000))
        assertEquals("[00:01.00]abc", LrcUtils.adjustLyricsTiming("[00:01.00]abc", 0))
        assertEquals(
            "[00:10.00]第一\n[00:11.00]第二\n",
            LrcUtils.adjustLyricsTiming("[00:09.00]第一\n[00:10.00]第二\n", 1000)
        )
    }

    @Test
    fun mergesBilingualLyrics() {
        val original = "[ti:稻香]\n[00:01.00]第一句\n[00:02.00]第二句\n"
        val translation = "[00:01.00]Line one\n[00:02.00]第二句\n[00:03.00]Only translation\n"
        val merged = LrcUtils.mergeBilingualLyrics(original, translation)
        assertEquals(
            "[ti:稻香]\n" +
                "[00:01.00]第一句\n" +
                "[00:01.00]Line one\n" +
                "[00:02.00]第二句\n" +
                "[00:03.00]Only translation\n",
            merged
        )
    }

    @Test
    fun computesNameSimilarity() {
        assertEquals(1.0, LrcUtils.nameSimilarity("周杰伦 稻香", "稻香"), 1e-9)
        assertEquals(1.0, LrcUtils.nameSimilarity("周杰伦 稻香", "周杰伦"), 1e-9)
        assertEquals(0.0, LrcUtils.nameSimilarity("abc", "xyz"), 1e-9)
        assertEquals(0.0, LrcUtils.nameSimilarity("", "xyz"), 1e-9)
        assertEquals(0.5, LrcUtils.nameSimilarity("Adele Hello", "Hello Rolling"), 1e-9)
    }

    @Test
    fun convertsToStandardFormat() {
        assertEquals("[00:01.50]abc", LrcUtils.convertToStandardFormat("[00:01:50]abc"))
        assertEquals("[01:02.34]abc", LrcUtils.convertToStandardFormat("[01:02.34]abc"))
    }

    @Test
    fun cleansEmptyTimestampLines() {
        assertEquals(
            "[00:01.00]a\n[00:03.00]c\n",
            LrcUtils.cleanLyrics("[00:01.00]a\n[00:02.00]\n[00:03.00]c\n")
        )
    }

    @Test
    fun parsesLrcKeepingDocumentOrderAndDuplicates() {
        val parsed = LrcUtils.parseLrc(
            "[00:10.00]第三\n[00:01.50]第一\n[00:10.00]也是第三\n[00:00.05]零\n"
        )
        assertEquals(
            listOf(
                LrcLine(10000L, "第三"),
                LrcLine(1500L, "第一"),
                LrcLine(10000L, "也是第三"),
                LrcLine(50L, "零")
            ),
            parsed
        )
    }

    @Test
    fun parsesLrcWithMultipleTimestampsOnOneLine() {
        val parsed = LrcUtils.parseLrc("[00:01.00][00:20.00]重复句\n")
        assertEquals(listOf(LrcLine(1000L, "重复句"), LrcLine(20000L, "重复句")), parsed)
    }
}
