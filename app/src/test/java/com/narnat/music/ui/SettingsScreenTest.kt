package com.narnat.music.ui

import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithText
import android.content.ContentProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.narnat.music.ui.screens.settings.LyricsPlatform
import com.narnat.music.ui.screens.settings.SettingsScreen
import com.narnat.music.ui.screens.settings.SettingsUiState
import com.narnat.music.ui.theme.NarTheme
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class SettingsScreenTest {

    @get:Rule
    val composeRule = createComposeRule()

    @Before
    fun initComposeResources() {
        val providerClass = Class.forName(COMPOSE_RESOURCES_PROVIDER) as Class<out ContentProvider>
        Robolectric.buildContentProvider(providerClass).create()
    }

    private fun render(state: SettingsUiState) {
        composeRule.setContent {
            NarTheme(dark = false) {
                SettingsScreen(
                    state = state,
                    onPlatformChange = {},
                    onDelayChange = {},
                    onDarkModeChange = {},
                    onSave = {},
                    onReset = {},
                    onOpenFolder = {}
                )
            }
        }
    }

    @Test
    fun downloadGroupIsRendered() {
        render(SettingsUiState())
        composeRule.onNodeWithText("下载设置").assertExists()
        composeRule.onNodeWithText("歌词提取平台").assertExists()
        composeRule.onNodeWithText("延迟 (ms)").assertExists()
    }

    @Test
    fun aboutGroupIsRendered() {
        render(SettingsUiState())
        composeRule.onNodeWithText("关于纳儿音乐").assertExists()
        composeRule.onNodeWithText("基于B站歌曲链接下载音乐文件").assertExists()
    }

    @Test
    fun platformValueAndSaveButtonsAreRendered() {
        render(SettingsUiState(platform = LyricsPlatform.Netease, musicCount = 7))
        composeRule.onAllNodesWithText("网易云音乐")[0].assertExists()
        composeRule.onNodeWithText("保存设置").assertExists()
        composeRule.onNodeWithText("恢复默认").assertExists()
        composeRule.onNodeWithText("7 首").assertExists()
    }

    @Test
    fun delayErrorIsRendered() {
        render(SettingsUiState(delayText = "abc", delayError = true))
        composeRule.onNodeWithText("延迟参数必须是整数").assertExists()
    }

    companion object {
        private const val COMPOSE_RESOURCES_PROVIDER =
            "org.jetbrains.compose.resources.AndroidContextProvider"
    }
}
