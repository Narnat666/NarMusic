package com.narnat.music.ui.components

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.narnat.music.ui.theme.LocalNarSemanticColors
import kotlinx.coroutines.delay

private const val TOAST_DURATION_MS = 3000L

enum class NarToastType { Info, Success, Warning, Error }

data class NarToastMessage(val id: Long, val text: String, val type: NarToastType)

class ToastState {

    var message: NarToastMessage? by mutableStateOf(null)
        private set

    private var counter = 0L

    fun show(text: String, type: NarToastType = NarToastType.Info) {
        counter += 1
        message = NarToastMessage(counter, text, type)
    }

    fun dismiss() {
        message = null
    }
}

@Composable
fun NarToastHost(state: ToastState, modifier: Modifier = Modifier, aboveBottom: Dp = 16.dp) {
    val current = state.message
    var last by remember { mutableStateOf<NarToastMessage?>(null) }
    LaunchedEffect(current?.id) {
        if (current != null) {
            last = current
            delay(TOAST_DURATION_MS)
            if (state.message?.id == current.id) state.dismiss()
        }
    }
    Box(
        modifier = modifier
            .fillMaxSize()
            .navigationBarsPadding()
            .padding(bottom = aboveBottom),
        contentAlignment = Alignment.BottomCenter
    ) {
        AnimatedVisibility(visible = current != null, enter = fadeIn(), exit = fadeOut()) {
            val shown = last ?: return@AnimatedVisibility
            val semantic = LocalNarSemanticColors.current
            val container = when (shown.type) {
                NarToastType.Info -> MaterialTheme.colorScheme.primaryContainer
                NarToastType.Success -> semantic.successContainer
                NarToastType.Warning -> semantic.warningContainer
                NarToastType.Error -> semantic.errorContainer
            }
            val content = when (shown.type) {
                NarToastType.Info -> MaterialTheme.colorScheme.onPrimaryContainer
                NarToastType.Success -> semantic.success
                NarToastType.Warning -> semantic.warning
                NarToastType.Error -> semantic.error
            }
            Surface(
                shape = RoundedCornerShape(12.dp),
                color = container.copy(alpha = 0.95f),
                shadowElevation = 4.dp,
                modifier = Modifier.padding(horizontal = 24.dp)
            ) {
                Box(modifier = Modifier.padding(horizontal = 20.dp, vertical = 14.dp)) {
                    Text(text = shown.text, color = content, style = MaterialTheme.typography.bodyMedium)
                }
            }
        }
    }
}
