package com.narnat.music.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.hasSetTextAction
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.narnat.music.ui.theme.NarCaptionText
import com.narnat.music.ui.theme.NarCoverThumb
import com.narnat.music.ui.theme.NarFillField
import com.narnat.music.ui.theme.NarGroup
import com.narnat.music.ui.theme.NarPillButton
import com.narnat.music.ui.theme.NarRow
import com.narnat.music.ui.theme.NarSectionTitle
import com.narnat.music.ui.theme.NarTextAction
import com.narnat.music.ui.theme.NarTheme
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class NarSurfacesTest {

    @get:Rule
    val composeRule = createComposeRule()

    @Test
    fun sectionTitleRendersText() {
        composeRule.setContent { NarTheme(dark = false) { NarSectionTitle("音乐提取") } }
        composeRule.onNodeWithText("音乐提取").assertIsDisplayed()
    }

    @Test
    fun pillButtonInvokesCallback() {
        var clicks = 0
        composeRule.setContent {
            NarTheme(dark = false) { NarPillButton(text = "开始下载", onClick = { clicks++ }) }
        }
        composeRule.onNodeWithText("开始下载").performClick()
        assertEquals(1, clicks)
    }

    @Test
    fun disabledPillButtonDoesNotInvokeCallback() {
        var clicks = 0
        composeRule.setContent {
            NarTheme(dark = false) {
                NarPillButton(text = "解析中...", onClick = { clicks++ }, enabled = false)
            }
        }
        composeRule.onNodeWithText("解析中...").performClick()
        assertEquals(0, clicks)
    }

    @Test
    fun textActionInvokesCallback() {
        var clicks = 0
        composeRule.setContent {
            NarTheme(dark = false) { NarTextAction(text = "保存到本地", onClick = { clicks++ }) }
        }
        composeRule.onNodeWithText("保存到本地").performClick()
        assertEquals(1, clicks)
    }

    @Test
    fun fillFieldShowsPlaceholderAndAcceptsInput() {
        var value = ""
        composeRule.setContent {
            NarTheme(dark = false) {
                NarFillField(
                    value = value,
                    onValueChange = { value = it },
                    placeholder = "输入B站歌曲视频链接或包含链接的文本..."
                )
            }
        }
        composeRule.onNodeWithText("输入B站歌曲视频链接或包含链接的文本...").assertIsDisplayed()
        composeRule.onNode(hasSetTextAction()).performTextInput("abc")
        assertEquals("abc", value)
    }

    @Test
    fun rowShowsTitleAndSubtitle() {
        composeRule.setContent {
            NarTheme(dark = false) {
                NarRow(title = "歌词提取平台", subtitle = "酷狗音乐")
            }
        }
        composeRule.onNodeWithText("歌词提取平台").assertIsDisplayed()
        composeRule.onNodeWithText("酷狗音乐").assertIsDisplayed()
    }

    @Test
    fun rowClickTriggersCallback() {
        var clicks = 0
        composeRule.setContent {
            NarTheme(dark = false) {
                NarRow(title = "深色模式", subtitle = "跟随系统", onClick = { clicks++ })
            }
        }
        composeRule.onNodeWithText("深色模式").performClick()
        assertEquals(1, clicks)
    }

    @Test
    fun coverThumbShowsLabel() {
        composeRule.setContent { NarTheme(dark = false) { NarCoverThumb(label = "07") } }
        composeRule.onNodeWithText("07").assertIsDisplayed()
    }

    @Test
    fun groupShowsTitleAndContent() {
        composeRule.setContent {
            NarTheme(dark = false) {
                NarGroup(title = "存储") {
                    NarRow(title = "音乐存放位置", subtitle = "Music/NarMusic")
                }
            }
        }
        composeRule.onNodeWithText("存储").assertIsDisplayed()
        composeRule.onNodeWithText("音乐存放位置").assertIsDisplayed()
    }

    @Test
    fun captionTextRendersText() {
        composeRule.setContent {
            NarTheme(dark = false) {
                Column { NarCaptionText("已下载") }
            }
        }
        composeRule.onNodeWithText("已下载").assertIsDisplayed()
    }
}
