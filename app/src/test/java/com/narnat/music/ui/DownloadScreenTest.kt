package com.narnat.music.ui

import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.narnat.music.ui.components.StatusLight
import com.narnat.music.ui.screens.download.DownloadScreen
import com.narnat.music.ui.screens.download.DownloadUiState
import com.narnat.music.ui.theme.NarTheme
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class DownloadScreenTest {

    @get:Rule
    val composeRule = createComposeRule()

    private fun render(state: DownloadUiState) {
        composeRule.setContent {
            NarTheme(dark = false) {
                DownloadScreen(
                    state = state,
                    onUrlChange = {},
                    onFilenameChange = {},
                    onStart = {},
                    onSaveToLocal = {}
                )
            }
        }
    }

    @Test
    fun idleStateShowsInputsWithoutStatusCard() {
        render(DownloadUiState())
        composeRule.onNodeWithText("音乐提取").assertExists()
        composeRule.onNodeWithText("音乐链接").assertExists()
        composeRule.onNodeWithText("开始下载").assertExists()
    }

    @Test
    fun downloadingStateShowsLiveGrid() {
        render(
            DownloadUiState(
                cardVisible = true,
                light = StatusLight.Downloading,
                taskStatusText = "下载中",
                downloadedText = "512.0 KB",
                speedText = "128.0 KB/s",
                timeText = "3.0秒",
                statusText = "进行中"
            )
        )
        composeRule.onNodeWithText("已下载").assertExists()
        composeRule.onNodeWithText("下载速度").assertExists()
        composeRule.onNodeWithText("下载时间").assertExists()
        composeRule.onNodeWithText("状态").assertExists()
        composeRule.onNodeWithText("512.0 KB").assertExists()
    }

    @Test
    fun doneStateShowsFileInfo() {
        render(
            DownloadUiState(
                cardVisible = true,
                light = StatusLight.Success,
                taskStatusText = "已完成",
                downloadedText = "3.2 MB",
                speedText = "0 B/s",
                timeText = "12.0秒",
                statusText = "已完成",
                fileInfoVisible = true,
                fileName = "晴天 - 周杰伦.m4a",
                fileSizeText = "3.2 MB",
                localButtonVisible = true
            )
        )
        composeRule.onNodeWithText("文件信息").assertExists()
        composeRule.onNodeWithText("文件名").assertExists()
        composeRule.onNodeWithText("晴天 - 周杰伦.m4a").assertExists()
        composeRule.onNodeWithText("保存到本地").assertExists()
    }

    @Test
    fun failedStateShowsErrorText() {
        render(
            DownloadUiState(
                cardVisible = true,
                light = StatusLight.Error,
                taskStatusText = "下载未完成",
                statusText = "未完成",
                error = "音频下载失败"
            )
        )
        composeRule.onNodeWithText("下载未完成").assertExists()
        composeRule.onNodeWithText("问题: 音频下载失败").assertExists()
        composeRule.onNodeWithText("未完成").assertExists()
    }
}
