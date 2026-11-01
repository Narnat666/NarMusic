package com.narnat.music.ui.spike

import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class SaltSpikeRenderTest {

    @get:Rule
    val composeRule = createComposeRule()

    @Test
    fun titleIsRendered() {
        composeRule.setContent { SaltSpikeScreen() }
        composeRule.onNodeWithText("纳儿音乐").assertExists()
        composeRule.onNodeWithText("歌词提取平台").assertExists()
    }

    @Test
    fun primaryButtonClickInvokesCallback() {
        var clicked = false
        composeRule.setContent {
            SaltSpikeScreen(onPrimaryAction = { clicked = true })
        }
        composeRule.onNodeWithText("开始下载").performClick()
        assertTrue(clicked)
    }

    @Test
    fun switcherClickTogglesState() {
        composeRule.setContent { SaltSpikeScreen() }
        composeRule.onNodeWithText("已关闭").assertExists()
        composeRule.onNodeWithText("歌词同步").performClick()
        composeRule.onNodeWithText("已开启").assertExists()
    }
}
