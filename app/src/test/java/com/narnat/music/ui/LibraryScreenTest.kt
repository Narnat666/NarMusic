package com.narnat.music.ui

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.test.assertCountEquals
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.longClick
import androidx.compose.ui.test.onAllNodesWithContentDescription
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTouchInput
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.narnat.music.ui.screens.library.LibraryItem
import com.narnat.music.ui.screens.library.LibraryScreen
import com.narnat.music.ui.screens.library.LibraryUiState
import com.narnat.music.ui.theme.NarTheme
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class LibraryScreenTest {

    @get:Rule
    val composeRule = createComposeRule()

    private val twoItems = listOf(
        LibraryItem(1, "晴天", "周杰伦 · 网易云音乐 · 3.2 MB", "2026-11-01 12:00"),
        LibraryItem(2, "夜曲", "周杰伦 · 酷狗音乐 · 3.4 MB", "2026-11-01 12:05")
    )

    private fun render(
        state: LibraryUiState,
        onEnterBatchSelect: (Long) -> Unit = {}
    ) {
        composeRule.setContent {
            NarTheme(dark = false) {
                LibraryScreen(
                    state = state,
                    onKeywordChange = {},
                    onClearKeyword = {},
                    onFilterChange = {},
                    onRefresh = {},
                    onToggleBatchMode = {},
                    onEnterBatchSelect = onEnterBatchSelect,
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

    @Test
    fun emptyLibraryShowsEmptyState() {
        render(LibraryUiState(loading = false, rawTotal = 0, items = emptyList()))
        composeRule.onNodeWithText("音乐库为空").assertExists()
        composeRule.onNodeWithText("前往下载页面添加音乐").assertExists()
        composeRule.onNodeWithText("0 首").assertExists()
    }

    @Test
    fun filteredEmptyShowsFilteredState() {
        render(LibraryUiState(loading = false, rawTotal = 3, items = emptyList(), keyword = "abc"))
        composeRule.onNodeWithText("未找到匹配的音乐").assertExists()
        composeRule.onNodeWithText("尝试其他关键词或筛选条件").assertExists()
    }

    @Test
    fun itemsAndCountAreRendered() {
        render(LibraryUiState(loading = false, rawTotal = 2, items = twoItems))
        composeRule.onNodeWithText("晴天").assertExists()
        composeRule.onNodeWithText("夜曲").assertExists()
        composeRule.onNodeWithText("2 首").assertExists()
        composeRule.onNodeWithText("周杰伦 · 网易云音乐 · 3.2 MB").assertExists()
    }

    @Test
    fun rowsShowCoverPlaceholderInsteadOfIndexLabel() {
        render(LibraryUiState(loading = false, rawTotal = 2, items = twoItems))
        composeRule.onNodeWithText("01").assertDoesNotExist()
        composeRule.onNodeWithText("02").assertDoesNotExist()
        composeRule.onNodeWithText("晴天").assertExists()
    }

    @Test
    fun loadingStateIsRendered() {
        render(LibraryUiState(loading = true, rawTotal = 0, items = emptyList()))
        composeRule.onNodeWithText("加载音乐库中...").assertExists()
    }

    @Test
    fun errorStateIsRendered() {
        render(LibraryUiState(loading = false, error = "数据库不可用", rawTotal = 0, items = emptyList()))
        composeRule.onNodeWithText("读取失败").assertExists()
        composeRule.onNodeWithText("数据库不可用").assertExists()
    }

    @Test
    fun batchStateShowsToolbar() {
        render(
            LibraryUiState(
                loading = false,
                rawTotal = 2,
                items = twoItems,
                batchMode = true,
                selectedIds = setOf(1L)
            )
        )
        composeRule.onNodeWithText("全选").assertExists()
        composeRule.onNodeWithText("已选 1 项").assertExists()
        composeRule.onNodeWithText("批量删除").assertExists()
    }

    @Test
    fun longPressEntersBatchSelect() {
        composeRule.setContent {
            var state by mutableStateOf(
                LibraryUiState(loading = false, rawTotal = 2, items = twoItems)
            )
            NarTheme(dark = false) {
                LibraryScreen(
                    state = state,
                    onKeywordChange = {},
                    onClearKeyword = {},
                    onFilterChange = {},
                    onRefresh = {},
                    onToggleBatchMode = {},
                    onEnterBatchSelect = { id ->
                        state = state.copy(batchMode = true, selectedIds = setOf(id))
                    },
                    onToggleSelect = { id, selected ->
                        state = state.copy(
                            selectedIds = if (selected) state.selectedIds + id
                            else state.selectedIds - id
                        )
                    },
                    onToggleSelectAll = {},
                    onPlay = {},
                    onDeleteOne = {},
                    onDeleteSelected = {},
                    onOpenFolder = {}
                )
            }
        }
        composeRule.onNodeWithText("晴天").performTouchInput { longClick() }
        composeRule.onNodeWithText("全选").assertExists()
        composeRule.onNodeWithText("已选 1 项").assertExists()
        composeRule.onNodeWithText("批量删除").assertExists()
    }

    @Test
    fun rowsOnlyExposePlayAndMoreIcons() {
        render(LibraryUiState(loading = false, rawTotal = 2, items = twoItems))
        composeRule.onAllNodesWithContentDescription("播放").assertCountEquals(2)
        composeRule.onAllNodesWithContentDescription("更多").assertCountEquals(2)
        composeRule.onAllNodesWithContentDescription("删除").assertCountEquals(0)
    }

    @Test
    fun moreMenuCarriesDeleteAndOpenFolder() {
        render(LibraryUiState(loading = false, rawTotal = 1, items = twoItems.take(1)))
        composeRule.onNodeWithContentDescription("更多").performClick()
        composeRule.onNodeWithText("打开文件位置").assertExists()
        composeRule.onNodeWithText("删除").assertExists()
    }
}
