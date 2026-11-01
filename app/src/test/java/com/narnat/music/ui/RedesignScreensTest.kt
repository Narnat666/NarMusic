package com.narnat.music.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.requiredSize
import androidx.compose.ui.Modifier
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.unit.dp
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.narnat.music.core.lyrics.LrcLine
import com.narnat.music.ui.components.StatusLight
import com.narnat.music.ui.player.PlayerScreen
import com.narnat.music.ui.player.PlayerUiState
import com.narnat.music.ui.screens.batch.BatchInputMode
import com.narnat.music.ui.screens.batch.BatchItem
import com.narnat.music.ui.screens.batch.BatchItemStatus
import com.narnat.music.ui.screens.batch.BatchScreen
import com.narnat.music.ui.screens.batch.BatchUiState
import com.narnat.music.ui.screens.download.DownloadScreen
import com.narnat.music.ui.screens.download.DownloadUiState
import com.narnat.music.ui.screens.library.LibraryFilter
import com.narnat.music.ui.screens.library.LibraryItem
import com.narnat.music.ui.screens.library.LibraryScreen
import com.narnat.music.ui.screens.library.LibraryUiState
import com.narnat.music.ui.screens.settings.DarkModeOption
import com.narnat.music.ui.screens.settings.LyricsPlatform
import com.narnat.music.ui.screens.settings.SettingsScreen
import com.narnat.music.ui.screens.settings.SettingsUiState
import com.narnat.music.ui.theme.NarTheme
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [34], qualifiers = "w411dp-h891dp-xhdpi")
class RedesignScreensTest {

    @get:Rule
    val composeRule = createComposeRule()

    private fun download(state: DownloadUiState) {
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
    fun downloadIdleShowsSinglePrimaryAction() {
        download(DownloadUiState())
        composeRule.onNodeWithText("音乐提取").assertExists()
        composeRule.onNodeWithText("开始下载").assertExists()
        composeRule.onNodeWithText("保存到本地").assertDoesNotExist()
    }

    @Test
    fun downloadDoneShowsLocalActionAndFileInfo() {
        download(
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
        composeRule.onNodeWithText("保存到本地").assertExists()
        composeRule.onNodeWithText("文件信息").assertExists()
        composeRule.onNodeWithText("晴天 - 周杰伦.m4a").assertExists()
    }

    @Test
    fun downloadBusyShowsSendingLabel() {
        download(DownloadUiState(sendBusy = true))
        composeRule.onNodeWithText("发送中...").assertExists()
        composeRule.onNodeWithText("开始下载").assertDoesNotExist()
    }

    @Test
    fun batchShowsModeTabsAndInputHint() {
        composeRule.setContent {
            NarTheme(dark = false) {
                BatchScreen(
                    state = BatchUiState(),
                    onModeChange = {},
                    onPasteChange = {},
                    onPickFile = {},
                    onRemoveFile = {},
                    onParse = {},
                    onClear = {},
                    onToggleSelectAll = {},
                    onToggleSelect = { _, _ -> },
                    onDeleteSelected = {},
                    onStartDownload = {},
                    onDeleteItem = {}
                )
            }
        }
        composeRule.onNodeWithText("批量下载").assertExists()
        composeRule.onNodeWithText("粘贴歌单").assertExists()
        composeRule.onNodeWithText("导入歌单").assertExists()
        composeRule.onNodeWithText("粘贴歌单内容").assertExists()
    }

    @Test
    fun batchResultsShowSummaryAndRows() {
        composeRule.setContent {
            NarTheme(dark = false) {
                BatchScreen(
                    state = BatchUiState(
                        resultsVisible = true,
                        totalCount = 2,
                        foundCount = 1,
                        notFoundCount = 1,
                        items = listOf(
                            BatchItem(1, "晴天", "周杰伦", BatchItemStatus.Found, false),
                            BatchItem(2, "夜曲", "周杰伦", BatchItemStatus.NotFound, false)
                        )
                    ),
                    onModeChange = {},
                    onPasteChange = {},
                    onPickFile = {},
                    onRemoveFile = {},
                    onParse = {},
                    onClear = {},
                    onToggleSelectAll = {},
                    onToggleSelect = { _, _ -> },
                    onDeleteSelected = {},
                    onStartDownload = {},
                    onDeleteItem = {}
                )
            }
        }
        composeRule.onNodeWithText("解析结果：2 首").assertExists()
        composeRule.onNodeWithText("已找到 1").assertExists()
        composeRule.onNodeWithText("未找到 1").assertExists()
    }

    @Test
    fun batchImportModeShowsDropZone() {
        composeRule.setContent {
            NarTheme(dark = false) {
                BatchScreen(
                    state = BatchUiState(inputMode = BatchInputMode.Import),
                    onModeChange = {},
                    onPasteChange = {},
                    onPickFile = {},
                    onRemoveFile = {},
                    onParse = {},
                    onClear = {},
                    onToggleSelectAll = {},
                    onToggleSelect = { _, _ -> },
                    onDeleteSelected = {},
                    onStartDownload = {},
                    onDeleteItem = {}
                )
            }
        }
        composeRule.onNodeWithText("点击或拖拽文件到此处").assertExists()
        composeRule.onNodeWithText("支持 .txt 文件").assertExists()
    }

    private val twoItems = listOf(
        LibraryItem(1, "晴天", "周杰伦 · 网易云音乐 · 3.2 MB", "2026-11-01 12:00"),
        LibraryItem(2, "夜曲", "周杰伦 · 酷狗音乐 · 3.4 MB", "2026-11-01 12:05")
    )

    @Test
    fun libraryListShowsRowsAndFilters() {
        composeRule.setContent {
            NarTheme(dark = false) {
                LibraryScreen(
                    state = LibraryUiState(rawTotal = 2, items = twoItems, filter = LibraryFilter.All),
                    onKeywordChange = {},
                    onClearKeyword = {},
                    onFilterChange = {},
                    onRefresh = {},
                    onToggleBatchMode = {},
                    onEnterBatchSelect = {},
                    onToggleSelect = { _, _ -> },
                    onToggleSelectAll = {},
                    onPlay = {},
                    onDeleteOne = {},
                    onDeleteSelected = {},
                    onOpenFolder = {}
                )
            }
        }
        composeRule.onNodeWithText("晴天").assertExists()
        composeRule.onNodeWithText("夜曲").assertExists()
        composeRule.onNodeWithText("A-Z").assertExists()
        composeRule.onNodeWithText("2 首").assertExists()
    }

    @Test
    fun libraryWideLayoutStillShowsRows() {
        composeRule.setContent {
            NarTheme(dark = false) {
                Box(modifier = Modifier.requiredSize(720.dp, 420.dp)) {
                    LibraryScreen(
                        state = LibraryUiState(rawTotal = 2, items = twoItems),
                        onKeywordChange = {},
                        onClearKeyword = {},
                        onFilterChange = {},
                        onRefresh = {},
                        onToggleBatchMode = {},
                        onEnterBatchSelect = {},
                        onToggleSelect = { _, _ -> },
                        onToggleSelectAll = {},
                        onPlay = {},
                        onDeleteOne = {},
                        onDeleteSelected = {},
                        onOpenFolder = {}
                    )
                }
            }
        }
        composeRule.onNodeWithText("晴天").assertExists()
        composeRule.onNodeWithText("夜曲").assertExists()
    }

    @Test
    fun libraryBatchModeShowsSelectionToolbar() {
        composeRule.setContent {
            NarTheme(dark = false) {
                LibraryScreen(
                    state = LibraryUiState(
                        rawTotal = 2,
                        items = twoItems,
                        batchMode = true,
                        selectedIds = setOf(1L)
                    ),
                    onKeywordChange = {},
                    onClearKeyword = {},
                    onFilterChange = {},
                    onRefresh = {},
                    onToggleBatchMode = {},
                    onEnterBatchSelect = {},
                    onToggleSelect = { _, _ -> },
                    onToggleSelectAll = {},
                    onPlay = {},
                    onDeleteOne = {},
                    onDeleteSelected = {},
                    onOpenFolder = {}
                )
            }
        }
        composeRule.onNodeWithText("全选").assertExists()
        composeRule.onNodeWithText("已选 1 项").assertExists()
        composeRule.onNodeWithText("批量删除").assertExists()
    }

    private fun settings(state: SettingsUiState) {
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
    fun settingsShowsGroupsAndRows() {
        settings(SettingsUiState(platform = LyricsPlatform.Kugou, musicCount = 3))
        composeRule.onNodeWithText("下载设置").assertExists()
        composeRule.onNodeWithText("歌词提取平台").assertExists()
        composeRule.onNodeWithText("外观").assertExists()
        composeRule.onNodeWithText("关于纳儿音乐").assertExists()
        composeRule.onNodeWithText("3 首").assertExists()
    }

    @Test
    fun settingsPlatformPickerOpensOptions() {
        settings(SettingsUiState(platform = LyricsPlatform.Kugou))
        composeRule.onNodeWithText("歌词提取平台").performClick()
        composeRule.onNodeWithText("网易云音乐").assertExists()
        composeRule.onNodeWithText("汽水音乐").assertExists()
    }

    @Test
    fun settingsDarkModeRowShowsCurrentValue() {
        settings(SettingsUiState(darkMode = DarkModeOption.On))
        composeRule.onNodeWithText("深色模式").assertExists()
        composeRule.onNodeWithText("开启").assertExists()
    }

    private fun player(state: PlayerUiState) {
        composeRule.setContent {
            NarTheme(dark = false) {
                PlayerScreen(
                    state = state,
                    coverBytes = null,
                    palette = null,
                    dark = false,
                    onDismiss = {},
                    onPlayPause = {},
                    onSeek = {},
                    onPrevious = {},
                    onNext = {},
                    hasPrevious = true,
                    hasNext = true
                )
            }
        }
    }

    @Test
    fun playerShowsTitleArtistAndHeader() {
        player(PlayerUiState(visible = true, title = "晴天", artist = "周杰伦", isPlaying = true))
        composeRule.onNodeWithText("正在播放").assertExists()
        composeRule.onNodeWithText("晴天").assertExists()
        composeRule.onNodeWithText("周杰伦").assertExists()
    }

    @Test
    fun playerWithoutLyricsShowsPlaceholder() {
        player(PlayerUiState(visible = true, title = "夜曲", lyrics = emptyList()))
        composeRule.onNodeWithText("暂无歌词").assertExists()
    }

    @Test
    fun playerWithLyricsRendersActiveLine() {
        player(
            PlayerUiState(
                visible = true,
                title = "稻香",
                lyrics = listOf(
                    LrcLine(0L, "第一行歌词"),
                    LrcLine(5000L, "第二行歌词")
                ),
                activeIndex = 1
            )
        )
        composeRule.onNodeWithText("第一行歌词").assertExists()
        composeRule.onNodeWithText("第二行歌词").assertExists()
    }
}
