package com.narnat.music.ui.screens.batch

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.narnat.music.ui.theme.NarCaptionText
import com.narnat.music.ui.theme.NarFillField
import com.narnat.music.ui.theme.NarHairline
import com.narnat.music.ui.theme.NarPagePadding
import com.narnat.music.ui.theme.NarPillButton
import com.narnat.music.ui.theme.NarSectionTitle
import com.narnat.music.ui.theme.NarSurface
import com.narnat.music.ui.theme.NarTextAction

enum class BatchInputMode { Paste, Import }

enum class BatchItemStatus { Found, NotFound, Downloading, Downloaded, Failed }

data class BatchItem(
    val index: Int,
    val title: String,
    val artistInfo: String,
    val status: BatchItemStatus,
    val exact: Boolean,
    val url: String = "",
    val platform: String = "",
    val delayMs: Int = 0,
    val filename: String = ""
)

data class BatchUiState(
    val inputMode: BatchInputMode = BatchInputMode.Paste,
    val pasteText: String = "",
    val importedFileName: String? = null,
    val parsing: Boolean = false,
    val progressHeader: String = "正在解析歌单...",
    val progressFraction: Float = 0f,
    val progressStats: String = "0 / 0",
    val resultsVisible: Boolean = false,
    val totalCount: Int = 0,
    val foundCount: Int = 0,
    val notFoundCount: Int = 0,
    val items: List<BatchItem> = emptyList(),
    val selectedIndexes: Set<Int> = emptySet()
)

@Composable
fun BatchScreen(
    state: BatchUiState,
    onModeChange: (BatchInputMode) -> Unit,
    onPasteChange: (String) -> Unit,
    onPickFile: () -> Unit,
    onRemoveFile: () -> Unit,
    onParse: () -> Unit,
    onClear: () -> Unit,
    onToggleSelectAll: (Boolean) -> Unit,
    onToggleSelect: (Int, Boolean) -> Unit,
    onDeleteSelected: () -> Unit,
    onStartDownload: () -> Unit,
    onDeleteItem: (Int) -> Unit
) {
    var confirmDelete by remember { mutableStateOf(false) }
    val compactHeight = LocalConfiguration.current.screenHeightDp < 480
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(NarPagePadding)
            .padding(bottom = 24.dp)
    ) {
        NarSectionTitle(
            text = "批量下载",
            modifier = Modifier.padding(horizontal = 4.dp, vertical = if (compactHeight) 2.dp else 8.dp)
        )
        Spacer(modifier = Modifier.height(if (compactHeight) 4.dp else 8.dp))
        BatchModeTabs(mode = state.inputMode, onChange = onModeChange)
        Spacer(modifier = Modifier.height(if (compactHeight) 10.dp else 16.dp))
        NarSurface(color = MaterialTheme.colorScheme.surface.copy(alpha = 0.86f)) {
            if (state.inputMode == BatchInputMode.Paste) {
                NarCaptionText(text = "粘贴歌单内容")
                Spacer(modifier = Modifier.height(10.dp))
                NarFillField(
                    value = state.pasteText,
                    onValueChange = onPasteChange,
                    minLines = if (compactHeight) 4 else 8,
                    placeholder = "每行一首，支持「歌名 - 歌手」或 B 站链接"
                )
            } else {
                BatchFileDropZone(
                    fileName = state.importedFileName,
                    onPick = onPickFile,
                    onRemove = onRemoveFile
                )
            }
        }
        Spacer(modifier = Modifier.height(16.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            NarPillButton(
                text = if (state.parsing) "解析中..." else "解析歌单",
                onClick = onParse,
                enabled = !state.parsing,
                modifier = Modifier.weight(1f)
            )
            Spacer(modifier = Modifier.width(8.dp))
            NarTextAction(
                text = "清空",
                onClick = onClear,
                enabled = !state.parsing,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        if (state.parsing) {
            Spacer(modifier = Modifier.height(16.dp))
            BatchParseProgress(state = state)
        }
        if (state.resultsVisible) {
            Spacer(modifier = Modifier.height(20.dp))
            BatchResultSummary(
                total = state.totalCount,
                found = state.foundCount,
                notFound = state.notFoundCount
            )
            Spacer(modifier = Modifier.height(12.dp))
            BatchEditToolbar(
                state = state,
                onToggleSelectAll = onToggleSelectAll,
                onStartDownload = onStartDownload,
                onDeleteSelected = { confirmDelete = true }
            )
            Spacer(modifier = Modifier.height(12.dp))
            if (state.items.isEmpty()) {
                Text(
                    text = "暂无搜索结果",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(vertical = 24.dp)
                )
            } else {
                NarSurface(
                    color = MaterialTheme.colorScheme.surface.copy(alpha = 0.86f),
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(vertical = 4.dp)
                ) {
                    state.items.forEachIndexed { position, item ->
                        if (position > 0) {
                            NarHairline(insetStart = 20.dp)
                        }
                        BatchItemRow(
                            item = item,
                            selected = item.index in state.selectedIndexes,
                            onSelect = { onToggleSelect(item.index, it) },
                            onDelete = { onDeleteItem(item.index) }
                        )
                    }
                }
            }
        }
    }
    if (confirmDelete) {
        AlertDialog(
            onDismissRequest = { confirmDelete = false },
            title = { Text(text = "删除确认") },
            text = { Text(text = "确定要删除选中的 ${state.selectedIndexes.size} 首歌曲吗？") },
            confirmButton = {
                TextButton(onClick = {
                    confirmDelete = false
                    onDeleteSelected()
                }) { Text(text = "确认") }
            },
            dismissButton = {
                TextButton(onClick = { confirmDelete = false }) { Text(text = "取消") }
            }
        )
    }
}
