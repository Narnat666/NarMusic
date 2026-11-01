package com.narnat.music.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.narnat.music.ui.theme.LocalNarSemanticColors
import com.narnat.music.ui.theme.NarTheme
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
@Config(sdk = [34])
class NarThemeTest {

    @get:Rule
    val composeRule = createComposeRule()

    @Test
    fun lightThemeUsesWebPalette() {
        var primary: Color? = null
        var background: Color? = null
        var success: Color? = null
        composeRule.setContent {
            NarTheme(dark = false) {
                primary = MaterialTheme.colorScheme.primary
                background = MaterialTheme.colorScheme.background
                success = LocalNarSemanticColors.current.success
                Text(text = "纳儿音乐")
            }
        }
        composeRule.onNodeWithText("纳儿音乐").assertExists()
        assertEquals(Color(0xFF2D7BC1), primary)
        assertEquals(Color(0xFFF8FAFD), background)
        assertEquals(Color(0xFF2E7D32), success)
    }

    @Test
    fun darkThemeUsesDerivedPalette() {
        var primary: Color? = null
        var background: Color? = null
        composeRule.setContent {
            NarTheme(dark = true) {
                primary = MaterialTheme.colorScheme.primary
                background = MaterialTheme.colorScheme.background
                Text(text = "纳儿音乐")
            }
        }
        composeRule.onNodeWithText("纳儿音乐").assertExists()
        assertEquals(Color(0xFFA3C9F0), primary)
        assertEquals(Color(0xFF121417), background)
    }

    @Test
    fun semanticColorsAreProvided() {
        var warning: Color? = null
        composeRule.setContent {
            NarTheme(dark = false) {
                warning = LocalNarSemanticColors.current.warning
            }
        }
        assertNotNull(warning)
        assertEquals(Color(0xFFF57C00), warning)
    }
}
