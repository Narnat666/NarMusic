package com.narnat.music.ui.player

import android.content.Context
import android.net.Uri
import androidx.core.net.toUri
import androidx.media3.common.MediaItem
import androidx.media3.exoplayer.ExoPlayer
import com.narnat.music.core.lyrics.LrcLine
import com.narnat.music.core.lyrics.LrcUtils
import com.narnat.music.core.mp4.Mp4TagReader
import com.narnat.music.data.db.MusicFileEntity
import com.narnat.music.ui.components.NarToastType
import com.narnat.music.ui.components.ToastState
import java.io.File
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

data class PlayerUiState(
    val visible: Boolean = false,
    val title: String = "未在播放",
    val artist: String = "纳儿音乐",
    val lyrics: List<LrcLine> = emptyList(),
    val activeIndex: Int = -1,
    val positionMs: Long = 0L,
    val durationMs: Long = 0L,
    val isPlaying: Boolean = false,
    val coverUri: String = ""
)

class PlayerController(
    private val context: Context,
    private val scope: CoroutineScope,
    private val toast: ToastState
) {

    private val _state = MutableStateFlow(PlayerUiState())
    val state: StateFlow<PlayerUiState> = _state.asStateFlow()

    private var player: ExoPlayer? = null
    private var queue: List<MusicFileEntity> = emptyList()
    private var index = -1
    private var ticker: Job? = null
    private var currentLyrics: List<LrcLine> = emptyList()

    fun open(items: List<MusicFileEntity>, id: Long) {
        val position = items.indexOfFirst { it.id == id }
        if (position < 0) {
            toast.show("未找到对应的音乐文件", NarToastType.Warning)
            return
        }
        queue = items
        index = position
        playCurrent()
    }

    fun toggle() {
        val player = player ?: return
        if (player.isPlaying) player.pause() else player.play()
        _state.update { it.copy(isPlaying = player.isPlaying) }
    }

    fun seekTo(positionMs: Long) {
        val player = player ?: return
        player.seekTo(positionMs)
        _state.update {
            it.copy(positionMs = positionMs, activeIndex = activeIndexAt(positionMs))
        }
    }

    fun next() {
        if (index + 1 >= queue.size) return
        index += 1
        playCurrent()
    }

    fun previous() {
        if (index <= 0) return
        index -= 1
        playCurrent()
    }

    fun hasNext(): Boolean = index + 1 < queue.size

    fun hasPrevious(): Boolean = index > 0

    fun close() {
        player?.pause()
        ticker?.cancel()
        ticker = null
        _state.update { it.copy(visible = false, isPlaying = false) }
    }

    fun release() {
        ticker?.cancel()
        ticker = null
        player?.release()
        player = null
    }

    private fun playCurrent() {
        val entity = queue.getOrNull(index) ?: return
        val player = ensurePlayer()
        runCatching {
            player.setMediaItem(MediaItem.fromUri(mediaUriOf(entity)))
            player.prepare()
            player.play()
        }.onFailure {
            toast.show("音频播放器初始化失败", NarToastType.Warning)
            return
        }
        currentLyrics = emptyList()
        _state.value = PlayerUiState(
            visible = true,
            title = titleOf(entity),
            artist = entity.artist.ifEmpty { "从音乐库播放" },
            lyrics = emptyList(),
            activeIndex = -1,
            positionMs = 0L,
            durationMs = 0L,
            isPlaying = true,
            coverUri = entity.mediaUri
        )
        loadLyrics(entity)
        startTicker()
    }

    private fun ensurePlayer(): ExoPlayer {
        player?.let { return it }
        val created = ExoPlayer.Builder(context).build()
        player = created
        return created
    }

    private fun mediaUriOf(entity: MusicFileEntity): Uri {
        val mediaUri = entity.mediaUri
        if (mediaUri.startsWith("content://") || mediaUri.startsWith("file://")) return mediaUri.toUri()
        return Uri.fromFile(File(entity.filePath))
    }

    private fun titleOf(entity: MusicFileEntity): String = when {
        entity.songName.isNotEmpty() -> entity.songName
        !entity.originalFilename.isNullOrEmpty() -> entity.originalFilename
        else -> "未知歌曲"
    }

    private fun loadLyrics(entity: MusicFileEntity) {
        scope.launch {
            toast.show("正在加载歌词...", NarToastType.Info)
            val text = runCatching {
                withContext(Dispatchers.IO) {
                    entity.lyrics?.takeIf { it.isNotBlank() }
                        ?: Mp4TagReader.read(File(entity.filePath)).lyrics
                }
            }.getOrElse {
                toast.show("歌词加载失败", NarToastType.Warning)
                ""
            }
            currentLyrics = if (text.isBlank()) emptyList() else LrcUtils.parseLrc(text)
            _state.update {
                it.copy(lyrics = currentLyrics, activeIndex = activeIndexAt(it.positionMs))
            }
            if (currentLyrics.isEmpty()) {
                toast.show("未找到歌词", NarToastType.Info)
            } else {
                toast.show("歌词加载成功", NarToastType.Success)
            }
        }
    }

    private fun startTicker() {
        ticker?.cancel()
        ticker = scope.launch {
            while (isActive) {
                delay(POSITION_POLL_MS)
                val current = player ?: continue
                val position = current.currentPosition
                val duration = current.duration.let { if (it > 0L) it else 0L }
                _state.update {
                    it.copy(
                        positionMs = position,
                        durationMs = duration,
                        isPlaying = current.isPlaying,
                        activeIndex = activeIndexAt(position)
                    )
                }
            }
        }
    }

    private fun activeIndexAt(positionMs: Long): Int =
        currentLyrics.indexOfLast { it.timeMs <= positionMs }

    companion object {
        private const val POSITION_POLL_MS = 200L
    }
}
