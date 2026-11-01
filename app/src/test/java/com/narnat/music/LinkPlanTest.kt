package com.narnat.music

import com.narnat.music.core.bili.LinkPlan
import com.narnat.music.core.bili.LinkPlan.Target
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LinkPlanTest {

    @Test
    fun shortLinkNeedsRedirectInsteadOfKeywordSearch() {
        val target = LinkPlan.plan("https://b23.tv/Ndg49HZ")
        assertTrue("短链必须走重定向，不能退化成搜索", target is Target.NeedRedirect)
        assertEquals("https://b23.tv/Ndg49HZ", (target as Target.NeedRedirect).url)
    }

    @Test
    fun shareTextWithShortLinkKeepsTheLink() {
        val input = "【百万级装备听《Counting Stars》- OneRepublic 【Hi-Res】-哔哩哔哩】 " +
            "https://b23.tv/Ndg49HZ"
        val target = LinkPlan.plan(input)
        assertTrue(target is Target.NeedRedirect)
        assertEquals("https://b23.tv/Ndg49HZ", (target as Target.NeedRedirect).url)
    }

    @Test
    fun fullLinkGivesBvidDirectly() {
        val target = LinkPlan.plan("https://www.bilibili.com/video/BV1aB4y1u7ox?p=1&t=3")
        assertEquals(Target.Bvid("BV1aB4y1u7ox"), target)
    }

    @Test
    fun bareBvidTextIsNotTreatedAsKeyword() {
        assertEquals(Target.Bvid("BV1aB4y1u7ox"), LinkPlan.plan("BV1aB4y1u7ox"))
    }

    @Test
    fun redirectedLinkGivesBvid() {
        val resolved = "https://www.bilibili.com/video/BV14v4y1Q7nG?buvid=XUE&p=1&share_source=COPY"
        assertEquals(Target.Bvid("BV14v4y1Q7nG"), LinkPlan.plan(resolved))
    }

    @Test
    fun avLinkGivesAid() {
        assertEquals(
            Target.Aid(170001),
            LinkPlan.plan("https://www.bilibili.com/video/av170001")
        )
    }

    @Test
    fun plainKeywordGoesToSearch() {
        assertEquals(Target.Keyword("周杰伦 稻香"), LinkPlan.plan("周杰伦 稻香"))
    }

    @Test
    fun emptyInputIsEmpty() {
        assertEquals(Target.Empty, LinkPlan.plan("   "))
    }

    @Test
    fun trailingChinesePunctuationIsStrippedFromLink() {
        assertEquals(
            "https://b23.tv/Ndg49HZ",
            com.narnat.music.core.bili.BiliUtils.resolveUrlInput(
                "看这个 https://b23.tv/Ndg49HZ，很好听。"
            )
        )
        assertEquals(
            "https://b23.tv/Ndg49HZ",
            com.narnat.music.core.bili.BiliUtils.resolveUrlInput("https://b23.tv/Ndg49HZ）")
        )
    }

    @Test
    fun shortLinkWithPunctuationStillNeedsRedirect() {
        val target = LinkPlan.plan("【音乐】https://b23.tv/Ndg49HZ，听完记得收藏")
        assertTrue(target is Target.NeedRedirect)
        assertEquals("https://b23.tv/Ndg49HZ", (target as Target.NeedRedirect).url)
    }
}
