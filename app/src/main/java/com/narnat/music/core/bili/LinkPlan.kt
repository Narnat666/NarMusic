package com.narnat.music.core.bili

/**
 * 把用户输入（链接 / 分享文本 / 关键词）解析成「要下载哪一个视频」的计划。
 *
 * 关键约束：只要输入里出现了链接，就绝不允许回退成关键词搜索 —— 否则用户明确指定的
 * 视频会被搜索结果的随机一条取代（b23.tv 短链曾因此下载成别的歌）。
 */
object LinkPlan {

    sealed interface Target {
        /** 直接拿到 BV 号。 */
        data class Bvid(val bvid: String) : Target

        /** 老式 av 号。 */
        data class Aid(val aid: Long) : Target

        /** 短链，需要先跟随重定向再重新计划。 */
        data class NeedRedirect(val url: String) : Target

        /** 输入里没有链接，按关键词搜索。 */
        data class Keyword(val keyword: String) : Target

        /** 输入为空。 */
        data object Empty : Target
    }

    fun plan(input: String): Target {
        val text = input.trim()
        if (text.isEmpty()) return Target.Empty
        val url = BiliUtils.resolveUrlInput(text)
        if (url == null) {
            BiliUtils.extractBvid(text)?.let { return Target.Bvid(it) }
            return Target.Keyword(text)
        }
        BiliUtils.extractBvid(url)?.let { return Target.Bvid(it) }
        BiliUtils.extractAvId(url)?.let { return Target.Aid(it) }
        return Target.NeedRedirect(url)
    }
}
