package com.narnat.music.ui

import android.app.Activity
import android.content.Context
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.IntentSenderRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.narnat.music.appContainer
import com.narnat.music.data.db.MusicFileEntity
import com.narnat.music.data.prefs.SettingsStore
import com.narnat.music.di.AppContainer
import com.narnat.music.ui.components.NarBottomBar
import com.narnat.music.ui.components.NarNavigationRail
import com.narnat.music.ui.components.NarTab
import com.narnat.music.ui.components.NarToastHost
import com.narnat.music.ui.components.NarToastType
import com.narnat.music.ui.components.NarTopBar
import com.narnat.music.ui.components.ToastState
import com.narnat.music.ui.player.PlayerController
import com.narnat.music.ui.player.PlayerScreen
import com.narnat.music.ui.player.rememberCoverBytes
import com.narnat.music.ui.screens.batch.BatchController
import com.narnat.music.ui.screens.batch.BatchScreen
import com.narnat.music.ui.screens.download.DownloadController
import com.narnat.music.ui.screens.download.DownloadScreen
import com.narnat.music.ui.screens.library.LibraryController
import com.narnat.music.ui.screens.library.LibraryScreen
import com.narnat.music.ui.screens.settings.DarkModeOption
import com.narnat.music.ui.screens.settings.LyricsPlatform
import com.narnat.music.ui.screens.settings.SettingsScreen
import com.narnat.music.ui.screens.settings.SettingsUiState
import com.narnat.music.ui.theme.NarImmersiveBackground
import com.narnat.music.ui.theme.NarTheme
import com.narnat.music.ui.theme.rememberNarPalette

private val WideContentMaxWidth = 640.dp

@Composable
fun NarMusicApp(container: AppContainer = LocalContext.current.appContainer) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val toast = remember { ToastState() }
    val settings = container.settingsStore
    val download = remember(container) { DownloadController(container, scope, toast) }
    val batch = remember(container) { BatchController(container, scope, toast) }
    val library = remember(container) { LibraryController(container, scope, toast) }
    val player = remember(container) { PlayerController(context, scope, toast) }

    val darkModeName by settings.darkMode.collectAsState()
    val platformLabel by settings.lyricsPlatform.collectAsState()
    val delayMs by settings.delayMs.collectAsState()
    val playerState by player.state.collectAsState()

    var tab by rememberSaveable { mutableStateOf(NarTab.Download) }
    var delayText by remember { mutableStateOf(delayMs.toString()) }
    var delayError by remember { mutableStateOf(false) }

    LaunchedEffect(delayMs) {
        delayText = delayMs.toString()
        delayError = false
    }

    val dark = when (DarkModeOption.fromName(darkModeName)) {
        DarkModeOption.Follow -> isSystemInDarkTheme()
        DarkModeOption.On -> true
        DarkModeOption.Off -> false
    }

    val pickPlaylistFile = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) batch.importFile(context, uri)
    }
    val authorizeDelete = rememberLauncherForActivityResult(
        ActivityResultContracts.StartIntentSenderForResult()
    ) { result ->
        library.onDeleteAuthorized(result.resultCode == Activity.RESULT_OK)
    }
    LaunchedEffect(library.pendingIntentSender) {
        val sender = library.pendingIntentSender
        if (sender != null) {
            library.consumePendingIntent()
            authorizeDelete.launch(IntentSenderRequest.Builder(sender).build())
        }
    }
    DisposableEffect(player) {
        onDispose { player.release() }
    }

    val coverBytes = rememberCoverBytes(if (playerState.visible) playerState.coverUri else "")
    val palette = rememberNarPalette(coverBytes)
    val configuration = LocalConfiguration.current

    val onDelay: (String) -> Unit = { text ->
        delayText = text
        val parsed = text.trim().toIntOrNull()
        delayError = parsed == null
        if (parsed != null) settings.setDelayMs(parsed)
    }
    val onSave: () -> Unit = {
        val parsed = delayText.trim().toIntOrNull()
        if (parsed == null) {
            toast.show("延迟参数必须是整数", NarToastType.Warning)
        } else {
            settings.setDelayMs(parsed)
            container.syncPreferredPlatform()
            toast.show("设置已保存", NarToastType.Success)
        }
    }
    val onReset: () -> Unit = {
        settings.setLyricsPlatform(SettingsStore.PLATFORM_KUGOU)
        settings.setDelayMs(0)
        container.syncPreferredPlatform()
        toast.show("设置已恢复默认", NarToastType.Info)
    }
    val onPickFile: () -> Unit = { pickPlaylistFile.launch(arrayOf("text/plain")) }

    val pages: @Composable (Boolean) -> Unit = { centered ->
        AppPages(
            tab = tab,
            context = context,
            container = container,
            toast = toast,
            library = library,
            download = download,
            batch = batch,
            player = player,
            delayText = delayText,
            delayError = delayError,
            platformLabel = platformLabel,
            darkModeName = darkModeName,
            onDelayTextChange = onDelay,
            onSaveSettings = onSave,
            onResetSettings = onReset,
            onPickPlaylistFile = onPickFile,
            centered = centered
        )
    }

    NarTheme(dark = dark) {
        val toastBottom = if (configuration.orientation ==
            android.content.res.Configuration.ORIENTATION_LANDSCAPE
        ) {
            16.dp
        } else {
            96.dp
        }
        Box(modifier = Modifier.fillMaxSize()) {
            NarImmersiveBackground(palette = palette, dark = dark) {
                BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
                    val wide = maxWidth >= 600.dp || configuration.orientation ==
                        android.content.res.Configuration.ORIENTATION_LANDSCAPE
                    if (wide) {
                        Row(modifier = Modifier.fillMaxSize()) {
                            NarNavigationRail(current = tab, onTabSelected = { tab = it })
                            Column(
                                modifier = Modifier
                                    .fillMaxSize()
                                    .windowInsetsPadding(WindowInsets.statusBars)
                                    .navigationBarsPadding()
                            ) {
                                pages(true)
                            }
                        }
                    } else {
                        Column(modifier = Modifier.fillMaxSize()) {
                            NarTopBar(
                                title = "纳儿音乐",
                                searchText = library.ui.keyword,
                                onSearchTextChange = { library.setKeyword(it) },
                                onSearchSubmit = { tab = NarTab.Library },
                                showSearch = tab != NarTab.Library
                            )
                            Box(modifier = Modifier.weight(1f)) {
                                pages(false)
                            }
                            NarBottomBar(current = tab, onTabSelected = { tab = it })
                        }
                    }
                }
            }
            NarToastHost(state = toast, aboveBottom = toastBottom)
            if (playerState.visible) {
                PlayerScreen(
                    state = playerState,
                    coverBytes = coverBytes,
                    palette = palette,
                    dark = dark,
                    onDismiss = { player.close() },
                    onPlayPause = { player.toggle() },
                    onSeek = { player.seekTo(it) },
                    onPrevious = { player.previous() },
                    onNext = { player.next() },
                    hasPrevious = player.hasPrevious(),
                    hasNext = player.hasNext()
                )
            }
            BackHandler(enabled = playerState.visible) { player.close() }
        }
    }
}

@Composable
private fun AppPages(
    tab: NarTab,
    context: Context,
    container: AppContainer,
    toast: ToastState,
    library: LibraryController,
    download: DownloadController,
    batch: BatchController,
    player: PlayerController,
    delayText: String,
    delayError: Boolean,
    platformLabel: String,
    darkModeName: String,
    onDelayTextChange: (String) -> Unit,
    onSaveSettings: () -> Unit,
    onResetSettings: () -> Unit,
    onPickPlaylistFile: () -> Unit,
    centered: Boolean
) {
    val content: @Composable () -> Unit = {
        when (tab) {
            NarTab.Download -> DownloadScreen(
                state = download.ui,
                onUrlChange = { download.onUrlChange(it) },
                onFilenameChange = { download.onFilenameChange(it) },
                onStart = { download.start() },
                onSaveToLocal = { download.saveToLocal(context) }
            )
            NarTab.Batch -> BatchScreen(
                state = batch.ui,
                onModeChange = { batch.setMode(it) },
                onPasteChange = { batch.onPasteChange(it) },
                onPickFile = onPickPlaylistFile,
                onRemoveFile = { batch.removeFile() },
                onParse = { batch.parse() },
                onClear = { batch.clear() },
                onToggleSelectAll = { batch.toggleSelectAll(it) },
                onToggleSelect = { index, selected -> batch.toggleSelect(index, selected) },
                onDeleteSelected = { batch.deleteSelected() },
                onStartDownload = { batch.startDownload() },
                onDeleteItem = { batch.deleteItem(it) }
            )
            NarTab.Library -> LibraryScreen(
                state = library.ui,
                onKeywordChange = { library.setKeyword(it) },
                onClearKeyword = { library.clearKeyword() },
                onFilterChange = { library.setFilter(it) },
                onRefresh = { library.refresh() },
                onToggleBatchMode = { library.toggleBatchMode() },
                onEnterBatchSelect = { library.enterBatchSelect(it) },
                onToggleSelect = { id, selected -> library.toggleSelect(id, selected) },
                onToggleSelectAll = { library.toggleSelectAll(it) },
                onPlay = { player.open(library.playQueue(), it.id) },
                onDeleteOne = { library.deleteOne(it) },
                onDeleteSelected = { library.deleteSelected() },
                onOpenFolder = { library.openFolder(context, it) }
            )
            NarTab.Settings -> SettingsScreen(
                state = SettingsUiState(
                    platform = LyricsPlatform.fromLabel(platformLabel),
                    delayText = delayText,
                    delayError = delayError,
                    darkMode = DarkModeOption.fromName(darkModeName),
                    musicCount = library.ui.rawTotal
                ),
                onPlatformChange = {
                    container.settingsStore.setLyricsPlatform(it.label)
                    container.syncPreferredPlatform()
                },
                onDelayChange = onDelayTextChange,
                onDarkModeChange = { option ->
                    container.settingsStore.setDarkMode(SettingsStore.DarkMode.valueOf(option.name))
                },
                onSave = onSaveSettings,
                onReset = onResetSettings,
                onOpenFolder = { openMusicFolder(context, container, toast) }
            )
        }
    }
    if (centered) {
        Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.TopCenter) {
            Box(modifier = Modifier.widthIn(max = WideContentMaxWidth).fillMaxSize()) {
                content()
            }
        }
    } else {
        content()
    }
}

private fun openMusicFolder(context: Context, container: AppContainer, toast: ToastState) {
    val probe = MusicFileEntity(
        filePath = "",
        systemFilename = "",
        downloadedAt = 0L,
        lastUsedAt = 0L
    )
    val intent = container.libraryRepository.openFolderIntent(probe)
    if (intent != null && runCatching { context.startActivity(intent) }.isSuccess) return
    toast.show("未找到可用的文件管理器", NarToastType.Warning)
}
