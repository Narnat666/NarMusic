package com.narnat.music

import com.narnat.music.data.remote.lyrics.LyricsAggregator
import com.narnat.music.data.remote.lyrics.LyricsProvider
import com.narnat.music.data.remote.lyrics.PlatformResult
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LyricsAggregatorTest {

    private val chineseLyrics = "[00:01.00]对这个世界如果你有太多的抱怨\n[00:02.00]跌倒了就不敢继续往前走\n"
    private val englishLyrics = "[00:06.22]Hello, it's me\n[00:11.32]I was wondering if after all these years\n"
    private val englishTranslation = "[00:06.22]你好 是我\n[00:11.32]我犹豫着要不要给你来电\n"

    private fun aggregator(
        providers: List<LyricsProvider>,
        preferred: String = ""
    ): LyricsAggregator = LyricsAggregator(
        providers = providers,
        preferredPlatform = preferred,
        dispatcher = Dispatchers.Unconfined,
        logger = { }
    )

    private fun provider(name: String, result: PlatformResult): LyricsProvider =
        object : LyricsProvider {
            override val name: String = name
            override suspend fun fetch(keyword: String): PlatformResult = result
        }

    // ---------- 第一级：指定平台核心数据齐全 ----------

    @Test
    fun picksPreferredPlatformWhenCoreDataPresent() = runBlocking {
        val kugou = provider(
            "酷狗音乐",
            PlatformResult(source = "酷狗音乐", lyrics = chineseLyrics, hasLyrics = true)
        )
        val netease = provider("网易云音乐", PlatformResult(source = "网易云音乐"))
        val qq = provider("QQ音乐", PlatformResult(source = "QQ音乐"))
        val qishui = provider("汽水音乐", PlatformResult(source = "汽水音乐"))

        val best = aggregator(listOf(kugou, netease, qq, qishui), "酷狗音乐").fetchBest("稻香")
        assertEquals("酷狗音乐", best.source)
        assertTrue(best.hasLyrics)
        assertEquals(chineseLyrics, best.lyrics)
    }

    // ---------- 第二级：按优先级选「核心数据 + 封面」齐全的平台 ----------

    @Test
    fun picksFirstCompletePlatformInOrder() = runBlocking {
        val cover = ByteArray(6000) { it.toByte() }
        val kugou = provider(
            "酷狗音乐",
            PlatformResult(source = "酷狗音乐", lyrics = chineseLyrics, hasLyrics = true)
        )
        val netease = provider(
            "网易云音乐",
            PlatformResult(
                source = "网易云音乐",
                lyrics = chineseLyrics,
                cover = cover,
                hasLyrics = true,
                hasCover = true
            )
        )
        val qq = provider("QQ音乐", PlatformResult(source = "QQ音乐"))
        val qishui = provider("汽水音乐", PlatformResult(source = "汽水音乐"))

        val best = aggregator(listOf(kugou, netease, qq, qishui)).fetchBest("稻香")
        assertEquals("网易云音乐", best.source)
        assertTrue(best.hasCover)
        assertTrue(cover.contentEquals(best.cover))
    }

    // ---------- 第三级：全都不完整时按 missingScore 兜底 ----------

    @Test
    fun fallsBackToHighestMissingScore() = runBlocking {
        val kugou = provider(
            "酷狗音乐",
            PlatformResult(
                source = "酷狗音乐",
                lyrics = chineseLyrics,
                album = "魔杰座",
                hasLyrics = true,
                hasAlbum = true
            )
        )
        val netease = provider("网易云音乐", PlatformResult(source = "网易云音乐"))
        val qq = provider("QQ音乐", PlatformResult(source = "QQ音乐"))
        val qishui = provider("汽水音乐", PlatformResult(source = "汽水音乐"))

        val best = aggregator(listOf(kugou, netease, qq, qishui)).fetchBest("稻香")
        assertEquals("酷狗音乐", best.source)
        assertEquals("魔杰座", best.album)
        assertFalse(best.hasCover)
    }

    // ---------- supplementMissing：跨平台补封面 + 补翻译 ----------

    @Test
    fun supplementsCoverAndTranslationFromOtherPlatforms() = runBlocking {
        val cover = ByteArray(8192) { (it * 3).toByte() }
        // QQ 有封面但英文歌缺翻译 → 不完整；网易云有翻译无封面 → 不完整；酷狗有专辑
        val kugou = provider(
            "酷狗音乐",
            PlatformResult(
                source = "酷狗音乐",
                lyrics = englishLyrics,
                album = "25",
                hasLyrics = true,
                hasAlbum = true
            )
        )
        val netease = provider(
            "网易云音乐",
            PlatformResult(
                source = "网易云音乐",
                lyrics = englishLyrics,
                translation = englishTranslation,
                hasLyrics = true,
                hasTranslation = true
            )
        )
        val qq = provider(
            "QQ音乐",
            PlatformResult(
                source = "QQ音乐",
                lyrics = englishLyrics,
                cover = cover,
                hasLyrics = true,
                hasCover = true
            )
        )
        val qishui = provider("汽水音乐", PlatformResult(source = "汽水音乐"))

        val best = aggregator(listOf(kugou, netease, qq, qishui)).fetchBest("Hello")
        assertEquals("网易云音乐", best.source)
        assertTrue("封面应从 QQ 音乐补齐", best.hasCover)
        assertTrue(cover.contentEquals(best.cover))
        assertEquals(englishTranslation, best.translation)
        assertTrue(best.hasTranslation)
    }

    @Test
    fun supplementsTranslationWhenPrimaryLacksIt() = runBlocking {
        val qq = provider(
            "QQ音乐",
            PlatformResult(
                source = "QQ音乐",
                lyrics = englishLyrics,
                album = "25",
                hasLyrics = true,
                hasAlbum = true
            )
        )
        val netease = provider(
            "网易云音乐",
            PlatformResult(
                source = "网易云音乐",
                translation = englishTranslation,
                hasTranslation = true
            )
        )
        val kugou = provider("酷狗音乐", PlatformResult(source = "酷狗音乐"))
        val qishui = provider("汽水音乐", PlatformResult(source = "汽水音乐"))

        val best = aggregator(listOf(kugou, netease, qq, qishui)).fetchBest("Hello")
        assertEquals("QQ音乐", best.source)
        assertEquals(englishTranslation, best.translation)
        assertTrue(best.hasTranslation)
    }

    // ---------- 单 Provider 抛异常不影响其它 ----------

    @Test
    fun ignoresFailingProvider() = runBlocking {
        val broken = object : LyricsProvider {
            override val name: String = "酷狗音乐"
            override suspend fun fetch(keyword: String): PlatformResult =
                throw IllegalStateException("boom")
        }
        val cover = ByteArray(6144) { it.toByte() }
        val netease = provider(
            "网易云音乐",
            PlatformResult(
                source = "网易云音乐",
                lyrics = chineseLyrics,
                cover = cover,
                hasLyrics = true,
                hasCover = true
            )
        )
        val qq = provider("QQ音乐", PlatformResult(source = "QQ音乐"))
        val qishui = provider("汽水音乐", PlatformResult(source = "汽水音乐"))

        val best = aggregator(listOf(broken, netease, qq, qishui)).fetchBest("稻香")
        assertEquals("网易云音乐", best.source)
        assertTrue(best.hasLyrics)
    }

    @Test
    fun returnsEmptyWhenNoProvider() = runBlocking {
        val best = aggregator(emptyList()).fetchBest("稻香")
        assertEquals("", best.source)
        assertFalse(best.hasLyrics)
    }
}
