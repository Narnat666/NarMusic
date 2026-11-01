package com.narnat.music.ui.screens.batch

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.List
import androidx.compose.material.icons.rounded.Clear
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material3.Checkbox
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.narnat.music.ui.theme.LocalNarSemanticColors
import com.narnat.music.ui.theme.NarCaptionText
import com.narnat.music.ui.theme.NarFieldShape
import com.narnat.music.ui.theme.NarPillButton
import com.narnat.music.ui.theme.NarTextAction

/** 输入方式切换：下划线式标签，不用灰底胶囊分段控件 */
@Composable
internal fun BatchModeTabs(mode: BatchInputMode, onChange: (BatchInputMode) -> Unit) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(28.dp)
    ) {
        BatchModeTab(
            text = "粘贴歌单",
            active = mode == BatchInputMode.Paste,
            onClick = { onChange(BatchInputMode.Paste) }
        )
        BatchModeTab(
            text = "导入歌单",
            active = mode == BatchInputMode.Import,
            onClick = { onChange(BatchInputMode.Import) }
        )
    }
}

@Composable
private fun BatchModeTab(text: String, active: Boolean, onClick: () -> Unit) {
    val colorScheme = MaterialTheme.colorScheme
    Column(
        modifier = Modifier
            .width(IntrinsicSize.Max)
            .clickable(onClick = onClick)
    ) {
        Text(
            text = text,
            fontSize = 15.sp,
            softWrap = false,
            maxLines = 1,
            fontWeight = if (active) FontWeight.Medium else FontWeight.Normal,
            color = if (active) colorScheme.primary else colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
            modifier = Modifier.padding(top = 8.dp, bottom = 6.dp)
        )
        Box(
            modifier = Modifier
                .height(2.dp)
                .fillMaxWidth(if (active) 1f else 0f)
                .clip(CircleShape)
                .background(if (active) colorScheme.primary else Color.Transparent)
        )
    }
}

@Composable
internal fun BatchFileDropZone(fileName: String?, onPick: () -> Unit, onRemove: () -> Unit) {
    val colorScheme = MaterialTheme.colorScheme
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(NarFieldShape)
            .background(colorScheme.primary.copy(alpha = 0.06f))
            .clickable(onClick = onPick)
            .padding(vertical = 28.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Icon(
            imageVector = Icons.AutoMirrored.Rounded.List,
            contentDescription = null,
            tint = colorScheme.primary,
            modifier = Modifier.size(32.dp)
        )
        Text(
            text = "点击或拖拽文件到此处",
            fontSize = 15.sp,
            fontWeight = FontWeight.Medium,
            color = colorScheme.onSurface,
            modifier = Modifier.padding(top = 12.dp)
        )
        Spacer(modifier = Modifier.height(4.dp))
        NarCaptionText(text = "支持 .txt 文件")
    }
    if (fileName != null) {
        Row(
            modifier = Modifier
                .padding(top = 12.dp)
                .fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = fileName,
                fontSize = 14.sp,
                color = colorScheme.onSurfaceVariant,
                modifier = Modifier.weight(1f)
            )
            IconButton(onClick = onRemove, modifier = Modifier.size(32.dp)) {
                Icon(
                    imageVector = Icons.Rounded.Clear,
                    contentDescription = "清除",
                    tint = colorScheme.onSurfaceVariant,
                    modifier = Modifier.size(16.dp)
                )
            }
        }
    }
}

@Composable
internal fun BatchParseProgress(state: BatchUiState) {
    val colorScheme = MaterialTheme.colorScheme
    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text(
                text = state.progressHeader,
                fontSize = 14.sp,
                color = colorScheme.onSurface
            )
            Text(
                text = state.progressStats,
                fontSize = 13.sp,
                color = colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
            )
        }
        Spacer(modifier = Modifier.height(10.dp))
        LinearProgressIndicator(
            progress = { state.progressFraction },
            modifier = Modifier
                .fillMaxWidth()
                .height(4.dp),
            color = colorScheme.primary,
            trackColor = colorScheme.primary.copy(alpha = 0.12f),
            strokeCap = StrokeCap.Round
        )
    }
}

@Composable
internal fun BatchResultSummary(total: Int, found: Int, notFound: Int) {
    val colorScheme = MaterialTheme.colorScheme
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
        modifier = Modifier.fillMaxWidth()
    ) {
        Text(
            text = "解析结果：$total 首",
            fontSize = 15.sp,
            fontWeight = FontWeight.Medium,
            color = colorScheme.onBackground
        )
        Spacer(modifier = Modifier.weight(1f))
        BatchBadge(text = "已找到 $found", foreground = colorScheme.primary)
        BatchBadge(
            text = "未找到 $notFound",
            foreground = LocalNarSemanticColors.current.error
        )
    }
}

@Composable
internal fun BatchEditToolbar(
    state: BatchUiState,
    onToggleSelectAll: (Boolean) -> Unit,
    onStartDownload: () -> Unit,
    onDeleteSelected: () -> Unit
) {
    val colorScheme = MaterialTheme.colorScheme
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Checkbox(
            checked = state.items.isNotEmpty() && state.selectedIndexes.size == state.items.size,
            onCheckedChange = onToggleSelectAll
        )
        Text(
            text = "全选",
            fontSize = 14.sp,
            color = colorScheme.onSurface
        )
        Spacer(modifier = Modifier.width(10.dp))
        Text(
            text = "已选 ${state.selectedIndexes.size} 项",
            fontSize = 13.sp,
            color = colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
        )
        Spacer(modifier = Modifier.weight(1f))
        NarTextAction(
            text = "删除选中",
            onClick = onDeleteSelected,
            enabled = state.selectedIndexes.isNotEmpty(),
            color = colorScheme.error
        )
    }
    Row(modifier = Modifier.fillMaxWidth()) {
        NarPillButton(
            text = "开始下载",
            onClick = onStartDownload,
            enabled = state.selectedIndexes.isNotEmpty(),
            modifier = Modifier.weight(1f)
        )
    }
}

@Composable
internal fun BatchItemRow(
    item: BatchItem,
    selected: Boolean,
    onSelect: (Boolean) -> Unit,
    onDelete: () -> Unit
) {
    val colorScheme = MaterialTheme.colorScheme
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Checkbox(
            checked = selected,
            onCheckedChange = onSelect,
            enabled = item.status != BatchItemStatus.NotFound
        )
        Spacer(modifier = Modifier.width(4.dp))
        Text(
            text = item.index.toString().padStart(2, '0'),
            fontSize = 13.sp,
            color = colorScheme.onSurfaceVariant.copy(alpha = 0.55f),
            modifier = Modifier.width(26.dp)
        )
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = item.title,
                fontSize = 16.sp,
                fontWeight = FontWeight.Medium,
                color = colorScheme.onSurface,
                maxLines = 1
            )
            Text(
                text = item.artistInfo,
                fontSize = 13.sp,
                color = colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                maxLines = 1
            )
        }
        BatchStatusBadge(item = item)
        IconButton(onClick = onDelete, modifier = Modifier.size(36.dp)) {
            Icon(
                imageVector = Icons.Rounded.Delete,
                contentDescription = "删除",
                tint = colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                modifier = Modifier.size(17.dp)
            )
        }
    }
}

@Composable
private fun BatchStatusBadge(item: BatchItem) {
    val semantic = LocalNarSemanticColors.current
    val colorScheme = MaterialTheme.colorScheme
    val text: String
    val foreground: Color
    if (item.exact) {
        text = "精确匹配"
        foreground = colorScheme.primary
    } else {
        when (item.status) {
            BatchItemStatus.Found -> {
                text = "已找到"
                foreground = semantic.success
            }
            BatchItemStatus.NotFound -> {
                text = "未找到"
                foreground = semantic.error
            }
            BatchItemStatus.Downloading -> {
                text = "下载中"
                foreground = semantic.warning
            }
            BatchItemStatus.Downloaded -> {
                text = "已下载"
                foreground = colorScheme.primary
            }
            BatchItemStatus.Failed -> {
                text = "失败"
                foreground = semantic.error
            }
        }
    }
    BatchBadge(text = text, foreground = foreground)
}

@Composable
private fun BatchBadge(text: String, foreground: Color) {
    Surface(shape = CircleShape, color = foreground.copy(alpha = 0.10f)) {
        Text(
            text = text,
            fontSize = 12.sp,
            color = foreground,
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 3.dp)
        )
    }
}
