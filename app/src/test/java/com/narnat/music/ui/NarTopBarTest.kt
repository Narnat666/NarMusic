package com.narnat.music.ui

import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.narnat.music.ui.components.NarTopBar
import com.narnat.music.ui.theme.NarTheme
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class NarTopBarTest {

    @get:Rule
    val composeRule = createComposeRule()

    private fun render(showSearch: Boolean = true) {
        composeRule.setContent {
            NarTheme(dark = false) {
                NarTopBar(
                    title = "纳儿音乐",
                    searchText = "",
                    onSearchTextChange = {},
                    onSearchSubmit = {},
                    showSearch = showSearch
                )
            }
        }
    }

    @Test
    fun searchFieldIsCollapsedByDefault() {
        render()
        composeRule.onNodeWithContentDescription("搜索").assertExists()
        composeRule.onNodeWithText("模糊搜索歌曲、歌手...").assertDoesNotExist()
    }

    @Test
    fun tappingSearchIconExpandsField() {
        render()
        composeRule.onNodeWithContentDescription("搜索").performClick()
        composeRule.onNodeWithText("模糊搜索歌曲、歌手...").assertExists()
    }

    @Test
    fun searchIconHiddenWhenSearchDisabled() {
        render(showSearch = false)
        composeRule.onNodeWithContentDescription("搜索").assertDoesNotExist()
        composeRule.onNodeWithText("模糊搜索歌曲、歌手...").assertDoesNotExist()
    }
}
