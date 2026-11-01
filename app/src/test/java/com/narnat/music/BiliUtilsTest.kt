package com.narnat.music

import com.narnat.music.core.bili.BiliUtils
import kotlinx.serialization.json.JsonPrimitive
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BiliUtilsTest {

    @Test
    fun extractsBvid() {
        assertEquals(
            "BV1aB4y1u7ox",
            BiliUtils.extractBvid("https://www.bilibili.com/video/BV1aB4y1u7ox?p=1")
        )
        assertEquals("BV1aB4y1u7ox", BiliUtils.extractBvid("BV1aB4y1u7ox"))
        assertNull(BiliUtils.extractBvid("https://www.bilibili.com/video"))
        assertNull(BiliUtils.extractBvid("BV1aB4"))
    }

    @Test
    fun resolvesFirstHttpLink() {
        assertEquals(
            "https://b23.tv/abc",
            BiliUtils.resolveUrlInput("看这个 https://b23.tv/abc 好听")
        )
        assertEquals(
            "https://www.bilibili.com/video/BV1aB4y1u7ox",
            BiliUtils.resolveUrlInput("https://www.bilibili.com/video/BV1aB4y1u7ox\n第二行")
        )
        assertNull(BiliUtils.resolveUrlInput("周杰伦 稻香"))
    }

    /** 手算校验值来自 `docs/api-probe.md` 第 3 节实测 URL 里的 `w_rid`。 */
    @Test
    fun signWbiMatchesProbeSample() {
        val imgKey = "7cd084941338484aae1ad9425b84077c"
        val subKey = "4932caff0ff746eab6f01bf08b70ac45"
        assertEquals("ea1db124af3c7062474693fa704f4ff8", BiliUtils.mixinKey(imgKey, subKey))

        val signed = BiliUtils.signWbi(
            mapOf("keyword" to "周杰伦", "page" to "1", "pagesize" to "20"),
            imgKey = imgKey,
            subKey = subKey,
            wts = 1793504764L
        )
        assertEquals("1793504764", signed["wts"])
        assertEquals("9f14f19763cb5b0ace54872de7ba1aef", signed["w_rid"])
    }

    @Test
    fun signsWbiWithSecondFixedSample() {
        val signed = BiliUtils.signWbi(
            mapOf("keyword" to "周杰伦", "page" to "1", "pagesize" to "20"),
            imgKey = "7cd084941338484aae1ad9425b84077c",
            subKey = "4932caff0ff746eab6f01bf08b70ac45",
            wts = 1793504772L
        )
        assertEquals("69b391ae220f15073808316c0e4b6950", signed["w_rid"])
    }

    @Test
    fun encodesWbiStrictly() {
        assertEquals("%E5%91%A8%E6%9D%B0%E4%BC%A6", BiliUtils.urlEncodeWbi("周杰伦"))
        assertEquals("a-b_c.d~e", BiliUtils.urlEncodeWbi("a-b_c.d~e"))
        assertEquals("a%20b", BiliUtils.urlEncodeWbi("a b"))
        assertEquals("%2B", BiliUtils.urlEncodeWbi("+"))
        assertEquals("%2F", BiliUtils.urlEncodeWbi("/"))
    }

    @Test
    fun optimizesKeyword() {
        assertEquals("周杰伦稻香", BiliUtils.optimizeKeyword("周杰伦 - 稻香"))
        assertEquals("周杰伦稻香", BiliUtils.optimizeKeyword("周杰伦_稻香"))
        assertEquals("周杰伦稻香", BiliUtils.optimizeKeyword("周杰伦·稻香"))
        assertEquals("周杰伦稻香", BiliUtils.optimizeKeyword("周杰伦—稻香"))
        assertEquals("周\u3000杰伦", BiliUtils.optimizeKeyword("周\u3000杰伦"))
        assertEquals("  ", BiliUtils.optimizeKeyword("  "))
    }

    @Test
    fun stripsHtmlTags() {
        assertEquals(
            "周杰伦稻香",
            BiliUtils.stripHtmlTags("<em class=\"keyword\">周杰伦</em>稻香")
        )
    }

    @Test
    fun parsesDurationText() {
        assertEquals(225, BiliUtils.parseDurationToSeconds("3:45"))
        assertEquals(3723, BiliUtils.parseDurationToSeconds("1:02:03"))
        assertEquals(25142, BiliUtils.parseDurationToSeconds("419:2"))
        assertEquals(223, BiliUtils.parseDurationToSeconds("223"))
        assertEquals(0, BiliUtils.parseDurationToSeconds(""))
    }

    @Test
    fun parsesDurationJsonElement() {
        assertEquals(201, BiliUtils.parseDurationToSeconds(JsonPrimitive(201)))
        assertEquals(225, BiliUtils.parseDurationToSeconds(JsonPrimitive("3:45")))
        assertEquals(3723, BiliUtils.parseDurationToSeconds(JsonPrimitive("1:02:03")))
    }

    @Test
    fun computesMd5Hex() {
        assertEquals("900150983cd24fb0d6963f7d28e17f72", BiliUtils.md5Hex("abc".toByteArray()))
        assertEquals("d41d8cd98f00b204e9800998ecf8427e", BiliUtils.md5Hex(""))
    }
}
