package com.narnat.music.domain

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PlaylistParserTest {

    @Test
    fun parsesFourSegmentNarmetaLine() {
        val items = PlaylistParser.parse("http://b23.tv/abc|网易云音乐|120|晴天 - 周杰伦")
        assertEquals(1, items.size)
        val item = items[0]
        assertTrue(item.exact)
        assertEquals("http://b23.tv/abc", item.url)
        assertEquals("网易云音乐", item.platform)
        assertEquals(120, item.delayMs)
        assertEquals("晴天 - 周杰伦", item.filename)
        assertEquals("晴天", item.title)
        assertEquals("周杰伦", item.artist)
    }

    @Test
    fun keepsPipeInFilenameForFiveSegments() {
        val items = PlaylistParser.parse("https://www.bilibili.com/video/BV1xx|酷狗音乐|-300|夜曲|Live 版 - 周杰伦")
        assertEquals(1, items.size)
        val item = items[0]
        assertTrue(item.exact)
        assertEquals(-300, item.delayMs)
        assertEquals("夜曲|Live 版 - 周杰伦", item.filename)
        assertEquals("夜曲|Live 版", item.title)
        assertEquals("周杰伦", item.artist)
    }

    @Test
    fun nonUrlFirstSegmentFallsBackToPlainParsing() {
        val items = PlaylistParser.parse("不是链接|网易云音乐|0|晴天 - 周杰伦")
        assertEquals(1, items.size)
        val item = items[0]
        assertFalse(item.exact)
        assertEquals("不是链接|网易云音乐|0|晴天", item.title)
        assertEquals("周杰伦", item.artist)
    }

    @Test
    fun splitsBySpaceDashSpace() {
        val items = PlaylistParser.parse("稻香 - 周杰伦")
        assertEquals("稻香", items[0].title)
        assertEquals("周杰伦", items[0].artist)
        assertEquals("稻香 周杰伦", items[0].keyword)
        assertFalse(items[0].exact)
    }

    @Test
    fun splitsBySingleDash() {
        val items = PlaylistParser.parse("七里香-周杰伦")
        assertEquals("七里香", items[0].title)
        assertEquals("周杰伦", items[0].artist)
    }

    @Test
    fun keepsRestWhenMultipleDashesPresent() {
        val items = PlaylistParser.parse("歌名 - 歌手 - 专辑")
        assertEquals("歌名", items[0].title)
        assertEquals("歌手 - 专辑", items[0].artist)
    }

    @Test
    fun singleDashKeepsRestOfLine() {
        val items = PlaylistParser.parse("A-B-C")
        assertEquals("A", items[0].title)
        assertEquals("B-C", items[0].artist)
    }

    @Test
    fun withoutSeparatorKeepsWholeLineAsTitle() {
        val items = PlaylistParser.parse("孤勇者")
        assertEquals("孤勇者", items[0].title)
        assertEquals("", items[0].artist)
        assertEquals("孤勇者", items[0].keyword)
    }

    @Test
    fun skipsEmptyAndConsecutiveBlankLines() {
        val items = PlaylistParser.parse("\n\n晴天 - 周杰伦\n\n\n   \n夜曲 - 周杰伦\n\n")
        assertEquals(2, items.size)
        assertEquals("晴天", items[0].title)
        assertEquals("夜曲", items[1].title)
    }

    @Test
    fun trimsEachLine() {
        val items = PlaylistParser.parse("   晴天 - 周杰伦   ")
        assertEquals("晴天", items[0].title)
        assertEquals("周杰伦", items[0].artist)
    }

    @Test
    fun mixedContentParsesInOrder() {
        val items = PlaylistParser.parse(
            "http://x.cn/1|QQ音乐|0|A - B\nC - D\nE"
        )
        assertEquals(3, items.size)
        assertTrue(items[0].exact)
        assertFalse(items[1].exact)
        assertEquals("C", items[1].title)
        assertEquals("E", items[2].title)
    }
}
