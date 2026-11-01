package com.narnat.music.ui.screens.download

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.narnat.music.ui.components.StatusLight
import com.narnat.music.ui.components.StatusLightDot
import com.narnat.music.ui.theme.NarCaptionText
import com.narnat.music.ui.theme.NarFillField
import com.narnat.music.ui.theme.NarHairline
import com.narnat.music.ui.theme.NarPagePadding
import com.narnat.music.ui.theme.NarPillButton
import com.narnat.music.ui.theme.NarSectionTitle
import com.narnat.music.ui.theme.NarSurface
import com.narnat.music.ui.theme.NarTextAction
import com.narnat.music.ui.theme.NarValueText

data class DownloadUiState(
    val urlText: String = "",
    val filenameText: String = "",
    val cardVisible: Boolean = false,
    val light: StatusLight = StatusLight.Idle,
    val taskStatusText: String = "等待中",
    val downloadedText: String = "0 B",
    val speedText: String = "0 B/s",
    val timeText: String = "-",
    val statusText: String = "-",
    val fileInfoVisible: Boolean = false,
    val fileName: String = "-",
    val fileSizeText: String = "-",
    val sendBusy: Boolean = false,
    val localButtonVisible: Boolean = false,
    val error: String = ""
)

@Composable
fun DownloadScreen(
    state: DownloadUiState,
    onUrlChange: (String) -> Unit,
    onFilenameChange: (String) -> Unit,
    onStart: () -> Unit,
    onSaveToLocal: () -> Unit
) {
    val compactHeight = LocalConfiguration.current.screenHeightDp < 480
    val action: @Composable () -> Unit = {
        Column(modifier = Modifier.fillMaxWidth()) {
            NarPillButton(
                text = if (state.sendBusy) "发送中..." else "开始下载",
                onClick = onStart,
                enabled = !state.sendBusy,
                modifier = Modifier.fillMaxWidth()
            )
            if (state.localButtonVisible) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.Center
                ) {
                    NarTextAction(text = "保存到本地", onClick = onSaveToLocal)
                }
            }
        }
    }
    val inputCard: @Composable () -> Unit = {
        NarSurface(
            color = MaterialTheme.colorScheme.surface.copy(alpha = 0.86f),
            contentPadding = PaddingValues(if (compactHeight) 12.dp else 20.dp)
        ) {
            NarCaptionText(text = "音乐链接")
            Spacer(modifier = Modifier.height(if (compactHeight) 6.dp else 10.dp))
            NarFillField(
                value = state.urlText,
                onValueChange = onUrlChange,
                placeholder = "输入B站歌曲视频链接或包含链接的文本...",
                minLines = if (compactHeight) 2 else 3
            )
            Spacer(modifier = Modifier.height(if (compactHeight) 8.dp else 20.dp))
            NarHairline()
            Spacer(modifier = Modifier.height(if (compactHeight) 8.dp else 20.dp))
            NarCaptionText(text = "自定义文件名（歌词提取）")
            Spacer(modifier = Modifier.height(if (compactHeight) 6.dp else 10.dp))
            NarFillField(
                value = state.filenameText,
                onValueChange = onFilenameChange,
                placeholder = "歌曲名称 - 作者",
                singleLine = true
            )
        }
    }

    if (compactHeight) {
        Row(
            modifier = Modifier
                .fillMaxSize()
                .padding(NarPagePadding),
            horizontalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Column(modifier = Modifier.weight(1f).fillMaxHeight()) {
                NarSectionTitle(
                    text = "音乐提取",
                    modifier = Modifier.padding(horizontal = 4.dp, vertical = 2.dp)
                )
                Spacer(modifier = Modifier.height(4.dp))
                Column(modifier = Modifier.weight(1f).verticalScroll(rememberScrollState())) {
                    inputCard()
                }
                Spacer(modifier = Modifier.height(8.dp))
                action()
            }
            Column(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxHeight()
                    .verticalScroll(rememberScrollState())
            ) {
                if (state.cardVisible) {
                    StatusCard(state = state)
                }
                Spacer(modifier = Modifier.height(8.dp))
            }
        }
        return
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(NarPagePadding)
    ) {
        Column(
            modifier = Modifier
                .weight(1f)
                .verticalScroll(rememberScrollState())
        ) {
            NarSectionTitle(
                text = "音乐提取",
                modifier = Modifier.padding(horizontal = 4.dp, vertical = 8.dp)
            )
            Spacer(modifier = Modifier.height(8.dp))
            inputCard()
            if (state.cardVisible) {
                Spacer(modifier = Modifier.height(20.dp))
                StatusCard(state = state)
            }
            Spacer(modifier = Modifier.height(16.dp))
        }
        Spacer(modifier = Modifier.height(14.dp))
        action()
    }
}

@Composable
private fun StatusCard(state: DownloadUiState) {
    val colorScheme = MaterialTheme.colorScheme
    NarSurface(color = colorScheme.surface.copy(alpha = 0.86f)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            StatusLightDot(light = state.light)
            Text(
                text = state.taskStatusText,
                fontSize = 17.sp,
                color = colorScheme.onSurface,
                modifier = Modifier.padding(start = 10.dp)
            )
        }
        if (state.error.isNotEmpty()) {
            Text(
                text = "问题: ${state.error}",
                style = MaterialTheme.typography.bodySmall,
                color = colorScheme.error,
                modifier = Modifier.padding(top = 8.dp)
            )
        }
        Spacer(modifier = Modifier.height(18.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(24.dp)) {
            StatusCell(label = "已下载", value = state.downloadedText, modifier = Modifier.weight(1f))
            StatusCell(label = "下载速度", value = state.speedText, modifier = Modifier.weight(1f))
        }
        Spacer(modifier = Modifier.height(18.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(24.dp)) {
            StatusCell(label = "下载时间", value = state.timeText, modifier = Modifier.weight(1f))
            StatusCell(label = "状态", value = state.statusText, modifier = Modifier.weight(1f))
        }
        if (state.fileInfoVisible) {
            Spacer(modifier = Modifier.height(18.dp))
            NarHairline()
            Spacer(modifier = Modifier.height(16.dp))
            Text(
                text = "文件信息",
                style = MaterialTheme.typography.labelMedium,
                color = colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
            )
            FileInfoRow(label = "文件名", value = state.fileName)
            FileInfoRow(label = "文件大小", value = state.fileSizeText)
        }
    }
}

@Composable
private fun StatusCell(label: String, value: String, modifier: Modifier = Modifier) {
    Column(modifier = modifier) {
        NarCaptionText(text = label)
        Spacer(modifier = Modifier.height(4.dp))
        NarValueText(text = value, size = 20)
    }
}

@Composable
private fun FileInfoRow(label: String, value: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 10.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        NarCaptionText(text = label)
        Text(
            text = value,
            fontSize = 15.sp,
            color = MaterialTheme.colorScheme.onSurface
        )
    }
}
