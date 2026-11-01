package com.narnat.music.ui.components

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.narnat.music.ui.theme.LocalNarSemanticColors

enum class StatusLight { Idle, Downloading, Success, Error }

@Composable
fun StatusLightDot(light: StatusLight, modifier: Modifier = Modifier) {
    val colors = LocalNarSemanticColors.current
    val baseColor = when (light) {
        StatusLight.Idle -> colors.idle
        StatusLight.Downloading -> colors.warning
        StatusLight.Success -> colors.success
        StatusLight.Error -> colors.error
    }
    val alpha = if (light == StatusLight.Downloading) {
        val transition = rememberInfiniteTransition(label = "statusLightPulse")
        val animated by transition.animateFloat(
            initialValue = 1f,
            targetValue = 0.35f,
            animationSpec = infiniteRepeatable(
                animation = tween(durationMillis = 1000, easing = LinearEasing),
                repeatMode = RepeatMode.Reverse
            ),
            label = "statusLightAlpha"
        )
        animated
    } else {
        1f
    }
    Box(
        modifier = modifier
            .size(12.dp)
            .background(baseColor.copy(alpha = alpha), CircleShape)
    )
}
