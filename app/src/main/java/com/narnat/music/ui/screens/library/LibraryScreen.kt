package com.narnat.music.ui.screens.library

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Clear
import androidx.compose.material.icons.rounded.FavoriteBorder
import androidx.compose.material.icons.rounded.MoreVert
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Search
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.narnat.music.ui.components.NarAlbumCover
import com.narnat.music.ui.components.NarEmptyState
import com.narnat.music.ui.components.NarErrorState
import com.narnat.music.ui.components.NarLoadingState
import com.narnat.music.ui.theme.NarCaptionText
import com.narnat.music.ui.theme.NarFillField
import com.narnat.music.ui.theme.NarHairline
import com.narnat.music.ui.theme.NarPagePadding
import com.narnat.music.ui.theme.NarSectionTitle
import com.narnat.music.ui.theme.NarSurface
import com.narnat.music.ui.theme.NarTextAction

enum class LibraryFilter(val label: String) {
    All("全部"),
    NameAsc("A-Z"),
    NameDesc("Z-A"),
    TimeNew("最新"),
    TimeOld("最早"),
    SizeLarge("最大"),
    SizeSmall("最小")
}

data class LibraryItem(
    val id: Long,
    val title: String,
    val subtitle: String,
    val timeText: String,
    val missing: Boolean = false,
    val coverSource: String = ""
)

data class LibraryUiState(
    val loading: Boolean = false,
    val error: String? = null,
    val rawTotal: Int = 0,
    val keyword: String = "",
    val filter: LibraryFilter = LibraryFilter.All,
    val items: List<LibraryItem> = emptyList(),
    val batchMode: Boolean = false,
    val selectedIds: Set<Long> = emptySet()
)

@Composable
fun LibraryScreen(
    state: LibraryUiState,
    onKeywordChange: (String) -> Unit,
    onClearKeyword: () -> Unit,
    onFilterChange: (LibraryFilter) -> Unit,
    onRefresh: () -> Unit,
    onToggleBatchMode: () -> Unit,
    onEnterBatchSelect: (Long) -> Unit,
    onToggleSelect: (Long, Boolean) -> Unit,
    onToggleSelectAll: (Boolean) -> Unit,
    onPlay: (LibraryItem) -> Unit,
    onDeleteOne: (LibraryItem) -> Unit,
    onDeleteSelected: () -> Unit,
    onOpenFolder: (LibraryItem) -> Unit
) {
    val colorScheme = MaterialTheme.colorScheme
    var confirmItem by remember { mutableStateOf<LibraryItem?>(null) }
    var confirmBatch by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(NarPagePadding)
    ) {
        NarSectionTitle(text = "音乐库", modifier = Modifier.padding(horizontal = 4.dp, vertical = 8.dp))
        Spacer(modifier = Modifier.height(8.dp))
        NarFillField(
            value = state.keyword,
            onValueChange = onKeywordChange,
            placeholder = "搜索歌曲名...",
            singleLine = true,
            leading = {
                Icon(
                    imageVector = Icons.Rounded.Search,
                    contentDescription = null,
                    tint = colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                    modifier = Modifier.size(18.dp)
                )
            },
            trailing = {
                if (state.keyword.isNotEmpty()) {
                    Icon(
                        imageVector = Icons.Rounded.Clear,
                        contentDescription = "清除",
                        tint = colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                        modifier = Modifier
                            .size(18.dp)
                            .combinedClickable(onClick = onClearKeyword)
                    )
                }
            },
            keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
            keyboardActions = KeyboardActions(onSearch = {})
        )
        Spacer(modifier = Modifier.height(10.dp))
        FilterRow(current = state.filter, onFilterChange = onFilterChange)
        Spacer(modifier = Modifier.height(6.dp))
        Row(verticalAlignment = Alignment.CenterVertically) {
            NarCaptionText(text = "${state.items.size} 首")
            Spacer(modifier = Modifier.weight(1f))
            NarTextAction(
                text = if (state.batchMode) "取消批量" else "批量选择",
                onClick = onToggleBatchMode
            )
            Spacer(modifier = Modifier.width(4.dp))
            NarTextAction(
                text = if (state.loading) "加载中..." else "刷新",
                onClick = onRefresh,
                color = colorScheme.onSurfaceVariant
            )
        }
        if (state.batchMode) {
            Spacer(modifier = Modifier.height(8.dp))
            LibraryBatchToolbar(
                state = state,
                onToggleSelectAll = onToggleSelectAll,
                onDeleteSelected = { confirmBatch = true }
            )
        }
        Spacer(modifier = Modifier.height(8.dp))
        when {
            state.loading && state.items.isEmpty() ->
                NarLoadingState(text = "加载音乐库中...", modifier = Modifier.weight(1f))
            state.error != null -> NarErrorState(
                title = "读取失败",
                subtitle = state.error,
                onRetry = onRefresh,
                modifier = Modifier.weight(1f).verticalScroll(rememberScrollState())
            )
            state.rawTotal == 0 -> NarEmptyState(
                icon = Icons.Rounded.FavoriteBorder,
                title = "音乐库为空",
                subtitle = "前往下载页面添加音乐",
                modifier = Modifier.weight(1f).verticalScroll(rememberScrollState())
            )
            state.items.isEmpty() -> NarEmptyState(
                icon = Icons.Rounded.Search,
                title = "未找到匹配的音乐",
                subtitle = "尝试其他关键词或筛选条件",
                modifier = Modifier.weight(1f).verticalScroll(rememberScrollState())
            )
            else -> NarSurface(
                modifier = Modifier.weight(1f),
                color = colorScheme.surface.copy(alpha = 0.86f),
                contentPadding = PaddingValues(vertical = 4.dp)
            ) {
                LazyColumn(modifier = Modifier.weight(1f)) {
                    itemsIndexed(state.items, key = { _, item -> item.id }) { index, item ->
                        if (index > 0) {
                            NarHairline(color = colorScheme.outlineVariant.copy(alpha = 0.5f))
                        }
                        LibraryRow(
                            item = item,
                            batchMode = state.batchMode,
                            selected = item.id in state.selectedIds,
                            onSelect = { onToggleSelect(item.id, it) },
                            onLongPress = { onEnterBatchSelect(item.id) },
                            onPlay = { onPlay(item) },
                            onDelete = { confirmItem = item },
                            onOpenFolder = { onOpenFolder(item) }
                        )
                    }
                }
            }
        }
    }
    confirmItem?.let { item ->
        AlertDialog(
            onDismissRequest = { confirmItem = null },
            title = { Text(text = "删除确认") },
            text = { Text(text = "确定删除 ${item.title}？") },
            confirmButton = {
                TextButton(onClick = {
                    confirmItem = null
                    onDeleteOne(item)
                }) { Text(text = "确认") }
            },
            dismissButton = {
                TextButton(onClick = { confirmItem = null }) { Text(text = "取消") }
            }
        )
    }
    if (confirmBatch) {
        AlertDialog(
            onDismissRequest = { confirmBatch = false },
            title = { Text(text = "删除确认") },
            text = { Text(text = "确定要删除选中的 ${state.selectedIds.size} 个文件吗？此操作不可恢复。") },
            confirmButton = {
                TextButton(onClick = {
                    confirmBatch = false
                    onDeleteSelected()
                }) { Text(text = "确认") }
            },
            dismissButton = {
                TextButton(onClick = { confirmBatch = false }) { Text(text = "取消") }
            }
        )
    }
}

@Composable
private fun FilterRow(current: LibraryFilter, onFilterChange: (LibraryFilter) -> Unit) {
    val colorScheme = MaterialTheme.colorScheme
    FlowRow(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(16.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        LibraryFilter.entries.forEach { filter ->
            val active = current == filter
            Text(
                text = filter.label,
                fontSize = 14.sp,
                fontWeight = if (active) FontWeight.Medium else FontWeight.Normal,
                color = if (active) colorScheme.primary else colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                modifier = Modifier.combinedClickable(onClick = { onFilterChange(filter) })
            )
        }
    }
}

@Composable
private fun LibraryBatchToolbar(
    state: LibraryUiState,
    onToggleSelectAll: (Boolean) -> Unit,
    onDeleteSelected: () -> Unit
) {
    val colorScheme = MaterialTheme.colorScheme
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(colorScheme.primary.copy(alpha = 0.06f), CircleShape)
            .padding(horizontal = 12.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Checkbox(
            checked = state.items.isNotEmpty() && state.selectedIds.size == state.items.size,
            onCheckedChange = onToggleSelectAll
        )
        Text(
            text = "全选",
            fontSize = 14.sp,
            color = colorScheme.onSurface
        )
        Spacer(modifier = Modifier.width(12.dp))
        Text(
            text = "已选 ${state.selectedIds.size} 项",
            fontSize = 13.sp,
            color = colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
        )
        Spacer(modifier = Modifier.weight(1f))
        TextButton(
            onClick = onDeleteSelected,
            enabled = state.selectedIds.isNotEmpty()
        ) {
            Text(
                text = "批量删除",
                fontSize = 14.sp,
                fontWeight = FontWeight.Medium,
                color = colorScheme.error
            )
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun LibraryRow(
    item: LibraryItem,
    batchMode: Boolean,
    selected: Boolean,
    onSelect: (Boolean) -> Unit,
    onLongPress: () -> Unit,
    onPlay: () -> Unit,
    onDelete: () -> Unit,
    onOpenFolder: () -> Unit
) {
    val colorScheme = MaterialTheme.colorScheme
    var menuVisible by remember { mutableStateOf(false) }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 72.dp)
            .combinedClickable(
                onClick = { if (batchMode) onSelect(!selected) else onPlay() },
                onLongClick = { if (!batchMode) onLongPress() }
            )
            .padding(start = 16.dp, end = 8.dp, top = 12.dp, bottom = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        if (batchMode) {
            Checkbox(checked = selected, onCheckedChange = onSelect)
        } else {
            NarAlbumCover(source = item.coverSource, size = 48.dp)
            Spacer(modifier = Modifier.width(14.dp))
        }
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = item.title,
                fontSize = 17.sp,
                fontWeight = FontWeight.Medium,
                color = if (item.missing) colorScheme.onSurfaceVariant else colorScheme.onSurface,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = item.subtitle,
                fontSize = 13.sp,
                color = colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
        if (!batchMode) {
            IconButton(onClick = onPlay, modifier = Modifier.size(36.dp)) {
                Icon(
                    imageVector = Icons.Rounded.PlayArrow,
                    contentDescription = "播放",
                    tint = colorScheme.primary,
                    modifier = Modifier.size(20.dp)
                )
            }
            Box {
                IconButton(onClick = { menuVisible = true }, modifier = Modifier.size(36.dp)) {
                    Icon(
                        imageVector = Icons.Rounded.MoreVert,
                        contentDescription = "更多",
                        tint = colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
                        modifier = Modifier.size(18.dp)
                    )
                }
                DropdownMenu(expanded = menuVisible, onDismissRequest = { menuVisible = false }) {
                    DropdownMenuItem(
                        text = { Text(text = "打开文件位置") },
                        onClick = {
                            menuVisible = false
                            onOpenFolder()
                        }
                    )
                    DropdownMenuItem(
                        text = { Text(text = "删除") },
                        onClick = {
                            menuVisible = false
                            onDelete()
                        }
                    )
                }
            }
        }
    }
}
