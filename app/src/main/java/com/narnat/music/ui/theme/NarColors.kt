package com.narnat.music.ui.theme

import androidx.compose.runtime.Immutable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color

@Immutable
data class NarSemanticColors(
    val success: Color,
    val successContainer: Color,
    val warning: Color,
    val warningContainer: Color,
    val error: Color,
    val errorContainer: Color,
    val idle: Color
)

val LightNarSemanticColors = NarSemanticColors(
    success = Color(0xFF2E7D32),
    successContainer = Color(0xFFE8F5E9),
    warning = Color(0xFFF57C00),
    warningContainer = Color(0xFFFFF3E0),
    error = Color(0xFFC62828),
    errorContainer = Color(0xFFFFEBEE),
    idle = Color(0xFFB0B0B0)
)

val DarkNarSemanticColors = NarSemanticColors(
    success = Color(0xFF7ED67A),
    successContainer = Color(0xFF1E4620),
    warning = Color(0xFFFFB870),
    warningContainer = Color(0xFF5A2E00),
    error = Color(0xFFFFB4AB),
    errorContainer = Color(0xFF93000A),
    idle = Color(0xFF6E7276)
)

val LocalNarSemanticColors = staticCompositionLocalOf { LightNarSemanticColors }
