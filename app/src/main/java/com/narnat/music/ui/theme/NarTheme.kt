@file:OptIn(com.moriafly.salt.ui.UnstableSaltUiApi::class)

package com.narnat.music.ui.theme

import android.app.Activity
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import com.moriafly.salt.ui.SaltDynamicColors
import com.moriafly.salt.ui.SaltShapes
import com.moriafly.salt.ui.SaltTheme
import com.moriafly.salt.ui.darkSaltColors
import com.moriafly.salt.ui.lightSaltColors
import com.moriafly.salt.ui.saltConfigs
import com.moriafly.salt.ui.saltDimens
import com.moriafly.salt.ui.saltTextStyles

private val NarSaltLightColors = lightSaltColors(
    androidx.compose.ui.graphics.Color(0xFF2D7BC1),
    androidx.compose.ui.graphics.Color(0xFF1A1A1A),
    androidx.compose.ui.graphics.Color(0xFF5A5D63),
    androidx.compose.ui.graphics.Color(0xFFF8FAFD),
    androidx.compose.ui.graphics.Color(0xFFFFFFFF),
    androidx.compose.ui.graphics.Color(0xFFFFFFFF),
    androidx.compose.ui.graphics.Color(0xFFE8EAED),
    androidx.compose.ui.graphics.Color(0xFFFFFFFF)
)

private val NarSaltDarkColors = darkSaltColors(
    androidx.compose.ui.graphics.Color(0xFFA3C9F0),
    androidx.compose.ui.graphics.Color(0xFFE3E6EA),
    androidx.compose.ui.graphics.Color(0xFFB4B8BE),
    androidx.compose.ui.graphics.Color(0xFF121417),
    androidx.compose.ui.graphics.Color(0xFF1A1C1F),
    androidx.compose.ui.graphics.Color(0xFF24272B),
    androidx.compose.ui.graphics.Color(0xFF2A2D31),
    androidx.compose.ui.graphics.Color(0xFF0B2F4A)
)

private val NarSaltDynamicColors = SaltDynamicColors(NarSaltLightColors, NarSaltDarkColors)

private val NarSaltTextStyles = saltTextStyles(
    TextStyle(fontSize = 16.sp),
    TextStyle(fontSize = 12.sp),
    TextStyle(fontSize = 16.sp, lineHeight = 24.sp)
)

private val NarSaltDimens = saltDimens(56.dp, 24.dp, 12.dp, 20.dp, 16.dp, 12.dp)

private val NarSaltShapes = SaltShapes(
    RoundedCornerShape(8.dp),
    RoundedCornerShape(16.dp),
    RoundedCornerShape(24.dp)
)

@Composable
fun NarTheme(dark: Boolean, content: @Composable () -> Unit) {
    val colorScheme = if (dark) NarDarkColorScheme else NarLightColorScheme
    val semanticColors = if (dark) DarkNarSemanticColors else LightNarSemanticColors
    val view = LocalView.current
    SideEffect {
        val window = (view.context as? Activity)?.window ?: return@SideEffect
        val controller = WindowCompat.getInsetsController(window, view)
        controller.isAppearanceLightStatusBars = !dark
        controller.isAppearanceLightNavigationBars = !dark
    }
    MaterialTheme(colorScheme = colorScheme, typography = NarTypography, shapes = NarShapes) {
        SaltTheme(
            saltConfigs(dark),
            NarSaltDynamicColors,
            NarSaltTextStyles,
            NarSaltDimens,
            NarSaltShapes
        ) {
            CompositionLocalProvider(LocalNarSemanticColors provides semanticColors) {
                content()
            }
        }
    }
}
