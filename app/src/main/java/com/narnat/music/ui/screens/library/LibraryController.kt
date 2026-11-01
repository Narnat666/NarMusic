package com.narnat.music.ui.screens.library

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.IntentSender
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.content.IntentCompat
import com.narnat.music.data.db.MusicFileEntity
import com.narnat.music.data.library.MusicLibraryRepository
import com.narnat.music.di.AppContainer
import com.narnat.music.ui.components.NarToastType
import com.narnat.music.ui.components.ToastState
import com.narnat.music.ui.components.formatBytes
import java.text.Collator
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch

class LibraryController(
    private val container: AppContainer,
    private val scope: CoroutineScope,
    private val toast: ToastState
) {

    var ui: LibraryUiState by mutableStateOf(LibraryUiState(loading = true))
        private set

    var pendingIntentSender: IntentSender? by mutableStateOf(null)
        private set

    private var source: List<MusicFileEntity> = emptyList()
    private var pendingEntity: MusicFileEntity? = null
    private val collator: Collator = Collator.getInstance(Locale.CHINA)
    private val dateFormat = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.CHINA)

    init {
        scope.launch {
            container.musicFileDao.observeAll().collect { list ->
                source = list
                ui = ui.copy(loading = false, error = null, rawTotal = list.size)
                recompute()
            }
        }
    }

    fun setKeyword(text: String) {
        ui = ui.copy(keyword = text)
        recompute()
    }

    fun clearKeyword() = setKeyword("")

    fun setFilter(filter: LibraryFilter) {
        ui = ui.copy(filter = filter)
        recompute()
    }

    fun refresh() {
        scope.launch {
            ui = ui.copy(loading = true)
            runCatching { container.libraryRepository.refresh() }
                .onFailure { ui = ui.copy(error = it.message ?: "未知错误") }
            ui = ui.copy(loading = false)
        }
    }

    fun toggleBatchMode() {
        ui = ui.copy(batchMode = !ui.batchMode, selectedIds = emptySet())
    }

    fun enterBatchSelect(id: Long) {
        ui = ui.copy(batchMode = true, selectedIds = setOf(id))
    }

    fun toggleSelect(id: Long, selected: Boolean) {
        ui = ui.copy(
            selectedIds = if (selected) ui.selectedIds + id else ui.selectedIds - id
        )
    }

    fun toggleSelectAll(selected: Boolean) {
        ui = ui.copy(selectedIds = if (selected) ui.items.map { it.id }.toSet() else emptySet())
    }

    fun deleteOne(item: LibraryItem) {
        requestDelete(item.id)
    }

    fun deleteSelected() {
        val ids = ui.selectedIds.toList()
        if (ids.isEmpty()) {
            toast.show("请先选择要删除的文件", NarToastType.Warning)
            return
        }
        scope.launch {
            var removed = 0
            var reason: String? = null
            for (id in ids) {
                val entity = source.firstOrNull { it.id == id } ?: continue
                when (val outcome = container.libraryRepository.deleteFile(entity)) {
                    is MusicLibraryRepository.DeleteOutcome.Deleted -> removed++
                    is MusicLibraryRepository.DeleteOutcome.NeedsUserConfirm -> {
                        pendingEntity = entity
                        pendingIntentSender = extractSender(outcome.intent)
                    }
                    is MusicLibraryRepository.DeleteOutcome.Failed -> reason = outcome.reason
                }
            }
            ui = ui.copy(selectedIds = emptySet(), batchMode = false)
            if (removed > 0) toast.show("已删除 $removed 个文件", NarToastType.Success)
            if (reason != null) toast.show("删除失败: $reason", NarToastType.Warning)
        }
    }

    fun onDeleteAuthorized(granted: Boolean) {
        val entity = pendingEntity
        pendingEntity = null
        if (!granted || entity == null) {
            toast.show("没有删除该文件的权限", NarToastType.Warning)
            return
        }
        scope.launch {
            when (val outcome = container.libraryRepository.deleteFile(entity)) {
                is MusicLibraryRepository.DeleteOutcome.Deleted ->
                    toast.show("已删除", NarToastType.Success)
                is MusicLibraryRepository.DeleteOutcome.Failed ->
                    toast.show("删除失败: ${outcome.reason}", NarToastType.Warning)
                is MusicLibraryRepository.DeleteOutcome.NeedsUserConfirm ->
                    toast.show("没有删除该文件的权限", NarToastType.Warning)
            }
        }
    }

    fun consumePendingIntent() {
        pendingIntentSender = null
    }

    fun openFolder(context: Context, item: LibraryItem) {
        val entity = source.firstOrNull { it.id == item.id } ?: return
        val intent = container.libraryRepository.openFolderIntent(entity)
        if (intent != null && runCatching { context.startActivity(intent) }.isSuccess) return
        val manager = context.getSystemService(Context.CLIPBOARD_SERVICE) as? ClipboardManager
        manager?.setPrimaryClip(ClipData.newPlainText("NarMusic", entity.filePath))
        toast.show("已复制路径到剪贴板", NarToastType.Info)
    }

    fun playQueue(): List<MusicFileEntity> =
        ui.items.mapNotNull { item -> source.firstOrNull { it.id == item.id } }

    fun entityOf(id: Long): MusicFileEntity? = source.firstOrNull { it.id == id }

    private fun requestDelete(id: Long) {
        val entity = source.firstOrNull { it.id == id } ?: return
        scope.launch {
            when (val outcome = container.libraryRepository.deleteFile(entity)) {
                is MusicLibraryRepository.DeleteOutcome.Deleted ->
                    toast.show("已删除", NarToastType.Success)
                is MusicLibraryRepository.DeleteOutcome.NeedsUserConfirm -> {
                    pendingEntity = entity
                    pendingIntentSender = extractSender(outcome.intent)
                }
                is MusicLibraryRepository.DeleteOutcome.Failed ->
                    toast.show("删除失败: ${outcome.reason}", NarToastType.Warning)
            }
        }
    }

    private fun extractSender(intent: android.content.Intent): IntentSender? =
        IntentCompat.getParcelableExtra(
            intent,
            MusicLibraryRepository.EXTRA_INTENT_SENDER,
            IntentSender::class.java
        )

    private fun recompute() {
        val keyword = ui.keyword.trim()
        val filtered = source.filter { entity ->
            keyword.isEmpty() ||
                entity.songName.contains(keyword, ignoreCase = true) ||
                entity.systemFilename.contains(keyword, ignoreCase = true) ||
                entity.artist.contains(keyword, ignoreCase = true)
        }
        val sorted = when (ui.filter) {
            LibraryFilter.All -> filtered
            LibraryFilter.NameAsc -> filtered.sortedWith { a, b -> collator.compare(titleOf(a), titleOf(b)) }
            LibraryFilter.NameDesc -> filtered.sortedWith { a, b -> collator.compare(titleOf(b), titleOf(a)) }
            LibraryFilter.TimeNew -> filtered.sortedByDescending { it.downloadedAt }
            LibraryFilter.TimeOld -> filtered.sortedBy { it.downloadedAt }
            LibraryFilter.SizeLarge -> filtered.sortedByDescending { it.fileSize }
            LibraryFilter.SizeSmall -> filtered.sortedBy { it.fileSize }
        }
        ui = ui.copy(loading = false, items = sorted.map { toItem(it) })
    }

    private fun toItem(entity: MusicFileEntity): LibraryItem = LibraryItem(
        id = entity.id,
        title = titleOf(entity),
        subtitle = subtitleOf(entity),
        timeText = dateFormat.format(Date(entity.downloadedAt * 1000L)),
        missing = entity.isMissing,
        coverSource = entity.mediaUri.ifEmpty { entity.filePath }
    )

    private fun titleOf(entity: MusicFileEntity): String = when {
        entity.songName.isNotEmpty() -> entity.songName
        !entity.originalFilename.isNullOrEmpty() -> entity.originalFilename
        entity.systemFilename.isNotEmpty() -> entity.systemFilename.substringBeforeLast('.')
        else -> "未知文件"
    }

    private fun subtitleOf(entity: MusicFileEntity): String {
        val sizeText = if (entity.fileSize > 0L) formatBytes(entity.fileSize) else "文件缺失"
        return listOf(entity.artist, entity.platform, sizeText)
            .filter { it.isNotBlank() }
            .joinToString(" · ")
    }
}
