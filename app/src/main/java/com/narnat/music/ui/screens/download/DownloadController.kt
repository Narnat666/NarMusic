package com.narnat.music.ui.screens.download

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import com.narnat.music.data.db.MusicFileEntity
import com.narnat.music.di.AppContainer
import com.narnat.music.domain.DownloadProgress
import com.narnat.music.domain.DownloadRequest
import com.narnat.music.ui.components.NarToastType
import com.narnat.music.ui.components.StatusLight
import com.narnat.music.ui.components.ToastState
import com.narnat.music.ui.components.formatBytes
import com.narnat.music.ui.components.formatElapsed
import com.narnat.music.ui.components.formatSpeed
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class DownloadController(
    private val container: AppContainer,
    private val scope: CoroutineScope,
    private val toast: ToastState
) {

    var ui: DownloadUiState by mutableStateOf(DownloadUiState())
        private set

    private var taskId: String? = null
    private var startMillis = 0L
    private var lastStage: DownloadProgress.Stage? = null
    private var finishedEntity: MusicFileEntity? = null

    init {
        scope.launch {
            container.downloadService.tasks.collect { onTasks(it) }
        }
        scope.launch {
            while (true) {
                delay(1000L)
                if (ui.cardVisible && !isTerminal()) refreshElapsed()
            }
        }
    }

    fun onUrlChange(text: String) {
        ui = ui.copy(urlText = text)
    }

    fun onFilenameChange(text: String) {
        ui = ui.copy(filenameText = text)
    }

    fun start() {
        if (ui.urlText.isBlank()) {
            toast.show("URL不能为空", NarToastType.Warning)
            return
        }
        val settings = container.settingsStore
        val request = DownloadRequest(
            urlOrKeyword = ui.urlText.trim(),
            filename = ui.filenameText.trim(),
            platform = settings.lyricsPlatform.value,
            delayMs = settings.delayMs.value
        )
        taskId = container.downloadService.start(request)
        startMillis = System.currentTimeMillis()
        lastStage = null
        finishedEntity = null
        ui = ui.copy(
            cardVisible = true,
            light = StatusLight.Idle,
            taskStatusText = "等待中",
            downloadedText = "0 B",
            speedText = "0 B/s",
            timeText = "-",
            statusText = "-",
            fileInfoVisible = false,
            fileName = "-",
            fileSizeText = "-",
            localButtonVisible = false,
            error = ""
        )
        toast.show("下载任务已创建，开始下载...", NarToastType.Success)
    }

    fun saveToLocal(context: Context) {
        val entity = finishedEntity
        if (entity == null) {
            toast.show("尚未完成下载", NarToastType.Warning)
            return
        }
        val intent = container.libraryRepository.openFolderIntent(entity)
        if (intent != null) {
            val launched = runCatching { context.startActivity(intent) }.isSuccess
            if (launched) return
        }
        copyPath(context, entity.filePath)
        toast.show("已复制路径到剪贴板", NarToastType.Info)
    }

    private fun copyPath(context: Context, path: String) {
        val manager = context.getSystemService(Context.CLIPBOARD_SERVICE) as? ClipboardManager
        manager?.setPrimaryClip(ClipData.newPlainText("NarMusic", path))
    }

    private fun isTerminal(): Boolean =
        lastStage == DownloadProgress.Stage.Done || lastStage == DownloadProgress.Stage.Failed

    private fun refreshElapsed() {
        val elapsed = System.currentTimeMillis() - startMillis
        ui = ui.copy(timeText = formatElapsed(elapsed))
    }

    private fun onTasks(tasks: List<DownloadProgress>) {
        val id = taskId ?: return
        val task = tasks.lastOrNull { it.taskId == id } ?: return
        val elapsed = System.currentTimeMillis() - startMillis
        val previous = lastStage
        lastStage = task.stage
        ui = ui.copy(
            cardVisible = true,
            light = lightOf(task.stage),
            taskStatusText = statusTextOf(task.stage),
            downloadedText = formatBytes(task.downloadedBytes),
            speedText = formatSpeed(task.speedBytesPerSec),
            timeText = formatElapsed(elapsed),
            statusText = gridStatusOf(task.stage),
            error = if (task.stage == DownloadProgress.Stage.Failed) task.error else ""
        )
        when {
            task.stage == DownloadProgress.Stage.Done && previous != DownloadProgress.Stage.Done -> {
                ui = ui.copy(localButtonVisible = true)
                toast.show("文件下载完成", NarToastType.Success)
                loadFinishedEntity(task.libraryId)
            }
            task.stage == DownloadProgress.Stage.Failed && previous != DownloadProgress.Stage.Failed -> {
                ui = ui.copy(localButtonVisible = false)
                toast.show("下载未完成: ${task.error}", NarToastType.Warning)
            }
        }
    }

    private fun loadFinishedEntity(libraryId: Long) {
        scope.launch {
            val entity = container.musicFileDao.findById(libraryId) ?: return@launch
            finishedEntity = entity
            ui = ui.copy(
                fileInfoVisible = true,
                fileName = entity.systemFilename.ifEmpty { "未命名文件" },
                fileSizeText = formatBytes(entity.fileSize)
            )
        }
    }

    private fun lightOf(stage: DownloadProgress.Stage): StatusLight = when (stage) {
        DownloadProgress.Stage.Done -> StatusLight.Success
        DownloadProgress.Stage.Failed -> StatusLight.Error
        DownloadProgress.Stage.Downloading,
        DownloadProgress.Stage.Lyrics,
        DownloadProgress.Stage.Packaging,
        DownloadProgress.Stage.Saving -> StatusLight.Downloading
        else -> StatusLight.Idle
    }

    private fun statusTextOf(stage: DownloadProgress.Stage): String = when (stage) {
        DownloadProgress.Stage.Done -> "已完成"
        DownloadProgress.Stage.Failed -> "下载未完成"
        DownloadProgress.Stage.Downloading,
        DownloadProgress.Stage.Lyrics,
        DownloadProgress.Stage.Packaging,
        DownloadProgress.Stage.Saving -> "下载中"
        else -> "等待中"
    }

    private fun gridStatusOf(stage: DownloadProgress.Stage): String = when (stage) {
        DownloadProgress.Stage.Done -> "已完成"
        DownloadProgress.Stage.Failed -> "未完成"
        DownloadProgress.Stage.Downloading,
        DownloadProgress.Stage.Lyrics,
        DownloadProgress.Stage.Packaging,
        DownloadProgress.Stage.Saving -> "进行中"
        else -> "-"
    }
}
