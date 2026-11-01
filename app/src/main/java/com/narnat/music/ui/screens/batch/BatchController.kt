package com.narnat.music.ui.screens.batch

import android.content.Context
import android.net.Uri
import android.provider.OpenableColumns
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.narnat.music.di.AppContainer
import com.narnat.music.domain.DownloadProgress
import com.narnat.music.domain.DownloadRequest
import com.narnat.music.domain.PlaylistItem
import com.narnat.music.domain.PlaylistParser
import com.narnat.music.ui.components.NarToastType
import com.narnat.music.ui.components.ToastState
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class BatchController(
    private val container: AppContainer,
    private val scope: CoroutineScope,
    private val toast: ToastState
) {

    var ui: BatchUiState by mutableStateOf(BatchUiState())
        private set

    private var importedText = ""
    private val downloadedUrls = HashMap<String, Int>()

    init {
        scope.launch {
            container.downloadService.tasks.collect { onTasks(it) }
        }
    }

    fun setMode(mode: BatchInputMode) {
        ui = ui.copy(inputMode = mode)
    }

    fun onPasteChange(text: String) {
        ui = ui.copy(pasteText = text)
    }

    fun importFile(context: Context, uri: Uri) {
        scope.launch {
            val name = queryDisplayName(context, uri) ?: uri.lastPathSegment.orEmpty()
            if (!name.endsWith(".txt", ignoreCase = true)) {
                toast.show("请上传 .txt 文件", NarToastType.Warning)
                return@launch
            }
            val text = withContext(Dispatchers.IO) {
                runCatching {
                    context.contentResolver.openInputStream(uri)?.use {
                        it.bufferedReader().readText()
                    }
                }.getOrNull()
            }
            if (text == null) {
                toast.show("文件读取失败", NarToastType.Warning)
                return@launch
            }
            importedText = text
            ui = ui.copy(importedFileName = name)
            toast.show("文件已导入: $name", NarToastType.Success)
        }
    }

    fun removeFile() {
        importedText = ""
        ui = ui.copy(importedFileName = null)
    }

    fun clear() {
        importedText = ""
        downloadedUrls.clear()
        ui = BatchUiState(inputMode = ui.inputMode)
        toast.show("已清空", NarToastType.Info)
    }

    fun parse() {
        if (ui.parsing) return
        val text = if (ui.inputMode == BatchInputMode.Paste) ui.pasteText.trim() else importedText.trim()
        if (text.isEmpty()) {
            toast.show("请先输入或导入歌单内容", NarToastType.Warning)
            return
        }
        var parsed = PlaylistParser.parse(text)
        if (parsed.isEmpty()) {
            toast.show("未解析到有效歌曲", NarToastType.Warning)
            return
        }
        if (parsed.size > MAX_ITEMS) {
            parsed = parsed.subList(0, MAX_ITEMS)
            toast.show("单次最多处理 $MAX_ITEMS 首，仅解析前 $MAX_ITEMS 首", NarToastType.Warning)
        }
        val pending = parsed
        scope.launch {
            ui = ui.copy(
                parsing = true,
                progressHeader = "正在解析歌单...",
                progressFraction = 0f,
                progressStats = "0 / ${pending.size}",
                resultsVisible = true,
                items = pending.mapIndexed { index, item ->
                    BatchItem(
                        index = index + 1,
                        title = item.title,
                        artistInfo = subTitleOf(item),
                        status = if (item.exact) BatchItemStatus.Found else BatchItemStatus.NotFound,
                        exact = item.exact,
                        url = item.url,
                        platform = item.platform,
                        delayMs = item.delayMs,
                        filename = item.filename
                    )
                },
                totalCount = pending.size,
                foundCount = pending.count { it.exact },
                notFoundCount = pending.size - pending.count { it.exact },
                selectedIndexes = emptySet()
            )
            if (pending.any { !it.exact }) {
                ui = ui.copy(progressHeader = "正在批量搜索歌单...")
            }
            var done = 0
            var exactCount = 0
            var found = 0
            var notFound = 0
            for ((position, item) in pending.withIndex()) {
                val itemIndex = position + 1
                if (item.exact) {
                    done++
                    exactCount++
                    found++
                } else {
                    val keyword = item.keyword
                    val hit = runCatching { container.biliClient.searchSingle(keyword) }.getOrNull()
                    delay(SEARCH_INTERVAL_MS)
                    done++
                    if (hit != null) {
                        found++
                        updateItem(itemIndex) {
                            it.copy(
                                status = BatchItemStatus.Found,
                                url = hit.link,
                                title = hit.title.ifEmpty { it.title }
                            )
                        }
                    } else {
                        notFound++
                    }
                }
                ui = ui.copy(
                    progressFraction = done.toFloat() / pending.size,
                    progressStats = "$done / ${pending.size}",
                    foundCount = found,
                    notFoundCount = notFound
                )
            }
            ui = ui.copy(parsing = false, progressFraction = 1f)
            val exactSuffix = if (exactCount > 0) "(含 $exactCount 首精确匹配)" else ""
            toast.show("解析完成: 找到 $found 首, 未找到 $notFound 首$exactSuffix", NarToastType.Info)
        }
    }

    fun toggleSelectAll(selected: Boolean) {
        ui = ui.copy(
            selectedIndexes = if (selected) {
                ui.items.filter { it.status != BatchItemStatus.NotFound }.map { it.index }.toSet()
            } else {
                emptySet()
            }
        )
    }

    fun toggleSelect(index: Int, selected: Boolean) {
        ui = ui.copy(
            selectedIndexes = if (selected) {
                ui.selectedIndexes + index
            } else {
                ui.selectedIndexes - index
            }
        )
    }

    fun deleteItem(index: Int) {
        val remaining = ui.items.filterNot { it.index == index }
        ui = ui.copy(
            items = reindex(remaining),
            selectedIndexes = emptySet(),
            totalCount = remaining.size,
            foundCount = remaining.count { it.status != BatchItemStatus.NotFound },
            notFoundCount = remaining.count { it.status == BatchItemStatus.NotFound }
        )
    }

    fun deleteSelected() {
        if (ui.selectedIndexes.isEmpty()) {
            toast.show("请先选择要删除的歌曲", NarToastType.Warning)
            return
        }
        val count = ui.selectedIndexes.size
        val remaining = ui.items.filterNot { it.index in ui.selectedIndexes }
        ui = ui.copy(
            items = reindex(remaining),
            selectedIndexes = emptySet(),
            totalCount = remaining.size,
            foundCount = remaining.count { it.status != BatchItemStatus.NotFound },
            notFoundCount = remaining.count { it.status == BatchItemStatus.NotFound }
        )
        toast.show("已删除 $count 首歌曲", NarToastType.Success)
    }

    fun startDownload() {
        if (ui.selectedIndexes.isEmpty()) {
            toast.show("请先选择要下载的歌曲", NarToastType.Warning)
            return
        }
        val settings = container.settingsStore
        val chosen = ui.items.filter { it.index in ui.selectedIndexes }
        for (item in chosen) {
            val request = DownloadRequest(
                urlOrKeyword = item.url.ifEmpty { item.title },
                filename = item.filename.ifEmpty { item.title },
                platform = item.platform.ifEmpty { settings.lyricsPlatform.value },
                delayMs = item.delayMs
            )
            val taskId = container.downloadService.start(request)
            downloadedUrls[taskId] = item.index
            updateItem(item.index) { it.copy(status = BatchItemStatus.Downloading) }
        }
        toast.show("已提交 ${chosen.size} 首歌曲下载", NarToastType.Info)
    }

    private fun onTasks(tasks: List<DownloadProgress>) {
        if (downloadedUrls.isEmpty()) return
        for (task in tasks) {
            val index = downloadedUrls[task.taskId] ?: continue
            when (task.stage) {
                DownloadProgress.Stage.Done -> {
                    updateItem(index) { it.copy(status = BatchItemStatus.Downloaded) }
                    downloadedUrls.remove(task.taskId)
                }
                DownloadProgress.Stage.Failed -> {
                    updateItem(index) { it.copy(status = BatchItemStatus.Failed) }
                    downloadedUrls.remove(task.taskId)
                }
                else -> Unit
            }
        }
    }

    private fun updateItem(index: Int, transform: (BatchItem) -> BatchItem) {
        ui = ui.copy(items = ui.items.map { if (it.index == index) transform(it) else it })
    }

    private fun reindex(items: List<BatchItem>): List<BatchItem> =
        items.mapIndexed { position, item -> item.copy(index = position + 1) }

    private fun subTitleOf(item: PlaylistItem): String = when {
        item.exact && item.platform.isNotEmpty() -> "${item.platform} · ${signedDelay(item.delayMs)}"
        item.artist.isNotEmpty() -> item.artist
        else -> "未知歌手"
    }

    private fun signedDelay(delayMs: Int): String = if (delayMs >= 0) "+${delayMs}ms" else "${delayMs}ms"

    private fun queryDisplayName(context: Context, uri: Uri): String? =
        runCatching {
            context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                if (!cursor.moveToFirst()) return@use null
                val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (index >= 0) cursor.getString(index) else null
            }
        }.getOrNull()

    companion object {
        private const val MAX_ITEMS = 100
        private const val SEARCH_INTERVAL_MS = 300L
    }
}
