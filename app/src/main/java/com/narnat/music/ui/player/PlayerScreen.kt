package com.narnat.music.ui.player

import android.content.Context
import android.media.MediaMetadataRetriever
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.KeyboardArrowLeft
import androidx.compose.material.icons.automirrored.rounded.KeyboardArrowRight
import androidx.compose.material.icons.rounded.KeyboardArrowDown
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.net.toUri
import coil.compose.AsyncImage
import com.narnat.music.ui.components.formatDuration
import com.narnat.music.ui.theme.NarCaptionText
import com.narnat.music.ui.theme.NarPalette
import com.narnat.music.ui.theme.NarTabularNumber
import com.narnat.music.ui.theme.playerBrush
import com.narnat.music.ui.theme.wash
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/** 全屏沉浸式播放器：竖屏单列、横屏左右双栏；背景由封面主色晕染 */
@Composable
fun PlayerScreen(
    state: PlayerUiState,
    coverBytes: ByteArray?,
    palette: NarPalette?,
    dark: Boolean,
    onDismiss: () -> Unit,
    onPlayPause: () -> Unit,
    onSeek: (Long) -> Unit,
    onPrevious: () -> Unit,
    onNext: () -> Unit,
    hasPrevious: Boolean,
    hasNext: Boolean
) {
    BoxWithConstraints(
        modifier = Modifier
            .fillMaxSize()
            .background(playerBrush(palette, dark))
    ) {
        val wide = maxWidth >= 600.dp
        val portraitArtSize = minOf(maxWidth - 96.dp, maxHeight * 0.38f)
        if (wide) {
            Row(
                modifier = Modifier
                    .fillMaxSize()
                    .statusBarsPadding()
                    .navigationBarsPadding()
                    .padding(horizontal = 24.dp, vertical = 12.dp)
            ) {
                Column(
                    modifier = Modifier
                        .width(330.dp)
                        .fillMaxHeight(),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    BoxWithConstraints(
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth()
                    ) {
                        val side = minOf(maxHeight, maxWidth - 110.dp, 240.dp)
                        Row(
                            modifier = Modifier.fillMaxSize(),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            PlayerArtwork(
                                coverBytes = coverBytes,
                                dark = dark,
                                modifier = Modifier.size(side)
                            )
                            Spacer(modifier = Modifier.width(12.dp))
                            TitleBlock(state = state, centered = false, titleSize = 22.sp)
                        }
                    }
                    ProgressBlock(state = state, onSeek = onSeek, inlineTimes = true)
                    Spacer(modifier = Modifier.height(2.dp))
                    ControlRow(
                        state = state,
                        onPlayPause = onPlayPause,
                        onPrevious = onPrevious,
                        onNext = onNext,
                        hasPrevious = hasPrevious,
                        hasNext = hasNext,
                        playSize = 52.dp,
                        stepSize = 44.dp,
                        modifier = Modifier.fillMaxWidth()
                    )
                }
                Spacer(modifier = Modifier.width(48.dp))
                Column(modifier = Modifier.weight(1f).fillMaxHeight()) {
                    LandscapeHeader(onDismiss = onDismiss)
                    LyricsPane(
                        lyrics = state.lyrics,
                        activeIndex = state.activeIndex,
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth()
                    )
                }
            }
        } else {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .navigationBarsPadding()
                    .padding(horizontal = 24.dp, vertical = 12.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                PlayerHeader(onDismiss = onDismiss)
                Spacer(modifier = Modifier.height(8.dp))
                PlayerArtwork(
                    coverBytes = coverBytes,
                    dark = dark,
                    modifier = Modifier.size(portraitArtSize)
                )
                Spacer(modifier = Modifier.height(20.dp))
                TitleBlock(state = state, centered = false)
                Spacer(modifier = Modifier.height(12.dp))
                LyricsPane(
                    lyrics = state.lyrics,
                    activeIndex = state.activeIndex,
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxWidth()
                )
                ProgressBlock(state = state, onSeek = onSeek)
                Spacer(modifier = Modifier.height(4.dp))
                ControlRow(
                    state = state,
                    onPlayPause = onPlayPause,
                    onPrevious = onPrevious,
                    onNext = onNext,
                    hasPrevious = hasPrevious,
                    hasNext = hasNext,
                    modifier = Modifier.fillMaxWidth()
                )
                Spacer(modifier = Modifier.height(8.dp))
            }
        }
    }
}

@Composable
private fun PlayerHeader(onDismiss: () -> Unit, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .statusBarsPadding(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        IconButton(onClick = onDismiss, modifier = Modifier.size(40.dp)) {
            Icon(
                imageVector = Icons.Rounded.KeyboardArrowDown,
                contentDescription = "收起播放器",
                tint = MaterialTheme.colorScheme.onSurface,
                modifier = Modifier.size(26.dp)
            )
        }
        Spacer(modifier = Modifier.weight(1f))
        NarCaptionText(text = "正在播放")
        Spacer(modifier = Modifier.weight(1f))
        Spacer(modifier = Modifier.size(40.dp))
    }
}

/** 横屏不做独占一行的顶栏，收起按钮与「正在播放」贴在歌词栏顶端，把高度让给封面 */
@Composable
private fun LandscapeHeader(onDismiss: () -> Unit, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        NarCaptionText(text = "正在播放")
        Spacer(modifier = Modifier.weight(1f))
        IconButton(onClick = onDismiss, modifier = Modifier.size(40.dp)) {
            Icon(
                imageVector = Icons.Rounded.KeyboardArrowDown,
                contentDescription = "收起播放器",
                tint = MaterialTheme.colorScheme.onSurface,
                modifier = Modifier.size(26.dp)
            )
        }
    }
}

@Composable
private fun PlayerArtwork(coverBytes: ByteArray?, dark: Boolean, modifier: Modifier = Modifier) {
    val colorScheme = MaterialTheme.colorScheme
    val shape = RoundedCornerShape(20.dp)
    Box(
        modifier = modifier
            .clip(shape)
            .background(
                wash(colorScheme.primary, colorScheme.surface, if (dark) 0.72f else 0.90f),
                shape
            )
            .semantics { contentDescription = "专辑封面" },
        contentAlignment = Alignment.Center
    ) {
        if (coverBytes != null) {
            AsyncImage(
                model = coverBytes,
                contentDescription = null,
                contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize().clip(shape)
            )
        } else {
            Box(
                modifier = Modifier
                    .size(72.dp)
                    .clip(CircleShape)
                    .background(colorScheme.primary.copy(alpha = 0.14f)),
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = Icons.Rounded.PlayArrow,
                    contentDescription = null,
                    tint = colorScheme.primary,
                    modifier = Modifier.size(40.dp)
                )
            }
        }
    }
}

@Composable
private fun TitleBlock(
    state: PlayerUiState,
    centered: Boolean,
    titleSize: TextUnit = 24.sp,
    artistSize: TextUnit = 14.sp
) {
    val align = if (centered) TextAlign.Center else TextAlign.Start
    Column(modifier = Modifier.fillMaxWidth()) {
        Text(
            text = state.title.ifEmpty { "未知歌曲" },
            fontSize = titleSize,
            lineHeight = titleSize * 1.34f,
            fontWeight = FontWeight.SemiBold,
            color = MaterialTheme.colorScheme.onSurface,
            textAlign = align,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
        Text(
            text = state.artist,
            fontSize = artistSize,
            color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.75f),
            textAlign = align,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
private fun TimeText(text: String) {
    Text(
        text = text,
        fontSize = 12.sp,
        style = NarTabularNumber.copy(fontSize = 12.sp),
        color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.75f),
        maxLines = 1
    )
}

@Composable
private fun ProgressBlock(
    state: PlayerUiState,
    onSeek: (Long) -> Unit,
    inlineTimes: Boolean = false
) {
    val colorScheme = MaterialTheme.colorScheme
    val colors = SliderDefaults.colors(
        thumbColor = colorScheme.primary,
        activeTrackColor = colorScheme.primary,
        inactiveTrackColor = colorScheme.onSurface.copy(alpha = 0.12f)
    )
    val range = 0f..(if (state.durationMs > 0L) state.durationMs.toFloat() else 1f)
    if (inlineTimes) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            TimeText(text = formatDuration(state.positionMs))
            Slider(
                value = state.positionMs.toFloat(),
                onValueChange = { onSeek(it.toLong()) },
                valueRange = range,
                colors = colors,
                modifier = Modifier
                    .weight(1f)
                    .padding(horizontal = 6.dp)
            )
            TimeText(text = formatDuration(state.durationMs))
        }
        return
    }
    Column(modifier = Modifier.fillMaxWidth()) {
        Slider(
            value = state.positionMs.toFloat(),
            onValueChange = { onSeek(it.toLong()) },
            valueRange = range,
            colors = colors
        )
        Row(modifier = Modifier.fillMaxWidth()) {
            TimeText(text = formatDuration(state.positionMs))
            Spacer(modifier = Modifier.weight(1f))
            TimeText(text = formatDuration(state.durationMs))
        }
    }
}

@Composable
private fun ControlRow(
    state: PlayerUiState,
    onPlayPause: () -> Unit,
    onPrevious: () -> Unit,
    onNext: () -> Unit,
    hasPrevious: Boolean,
    hasNext: Boolean,
    modifier: Modifier = Modifier,
    playSize: Dp = 60.dp,
    stepSize: Dp = 48.dp
) {
    val colorScheme = MaterialTheme.colorScheme
    Row(
        modifier = modifier,
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.Center
    ) {
        IconButton(
            onClick = onPrevious,
            enabled = hasPrevious,
            modifier = Modifier.size(stepSize)
        ) {
            Icon(
                imageVector = Icons.AutoMirrored.Rounded.KeyboardArrowLeft,
                contentDescription = "上一首",
                tint = colorScheme.onSurface.copy(alpha = if (hasPrevious) 0.85f else 0.25f),
                modifier = Modifier.size(stepSize * 0.62f)
            )
        }
        Spacer(modifier = Modifier.width(28.dp))
        Box(
            modifier = Modifier
                .size(playSize)
                .clip(CircleShape)
                .background(colorScheme.primary),
            contentAlignment = Alignment.Center
        ) {
            IconButton(
                onClick = onPlayPause,
                modifier = Modifier
                    .size(playSize)
                    .semantics { contentDescription = if (state.isPlaying) "暂停" else "播放" }
            ) {
                if (state.isPlaying) {
                    PauseGlyph(color = colorScheme.onPrimary)
                } else {
                    Icon(
                        imageVector = Icons.Rounded.PlayArrow,
                        contentDescription = null,
                        tint = colorScheme.onPrimary,
                        modifier = Modifier.size(playSize * 0.56f)
                    )
                }
            }
        }
        Spacer(modifier = Modifier.width(28.dp))
        IconButton(
            onClick = onNext,
            enabled = hasNext,
            modifier = Modifier.size(stepSize)
        ) {
            Icon(
                imageVector = Icons.AutoMirrored.Rounded.KeyboardArrowRight,
                contentDescription = "下一首",
                tint = colorScheme.onSurface.copy(alpha = if (hasNext) 0.85f else 0.25f),
                modifier = Modifier.size(stepSize * 0.62f)
            )
        }
    }
}

@Composable
private fun PauseGlyph(color: Color) {
    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Box(
            modifier = Modifier
                .size(width = 7.dp, height = 26.dp)
                .clip(RoundedCornerShape(2.dp))
                .background(color)
        )
        Box(
            modifier = Modifier
                .size(width = 7.dp, height = 26.dp)
                .clip(RoundedCornerShape(2.dp))
                .background(color)
        )
    }
}

@Composable
private fun LyricsPane(
    lyrics: List<com.narnat.music.core.lyrics.LrcLine>,
    activeIndex: Int,
    modifier: Modifier = Modifier
) {
    val colorScheme = MaterialTheme.colorScheme
    if (lyrics.isEmpty()) {
        Box(modifier = modifier, contentAlignment = Alignment.Center) {
            Text(
                text = "暂无歌词",
                fontSize = 14.sp,
                color = colorScheme.onSurfaceVariant.copy(alpha = 0.6f)
            )
        }
        return
    }
    val listState = rememberLazyListState()
    LaunchedEffect(activeIndex) {
        if (activeIndex >= 0) listState.animateScrollToItem(activeIndex)
    }
    Box(modifier = modifier, contentAlignment = Alignment.Center) {
        LazyColumn(
            state = listState,
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(14.dp),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(vertical = 48.dp)
        ) {
            itemsIndexed(lyrics) { position, line ->
                val active = position == activeIndex
                Text(
                    text = line.text.ifEmpty { " " },
                    fontSize = if (active) 17.sp else 15.sp,
                    lineHeight = 24.sp,
                    fontWeight = if (active) FontWeight.Bold else FontWeight.Normal,
                    color = if (active) {
                        colorScheme.onSurface
                    } else {
                        colorScheme.onSurfaceVariant.copy(alpha = 0.55f)
                    },
                    textAlign = TextAlign.Center,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 8.dp)
                )
            }
        }
    }
}

/** 从音频内嵌封面读取字节，供主色提取与封面显示复用 */
@Composable
fun rememberCoverBytes(coverUri: String): ByteArray? {
    val context = LocalContext.current
    val picture by produceState<ByteArray?>(initialValue = null, coverUri) {
        value = withContext(Dispatchers.IO) { embeddedPicture(context, coverUri) }
    }
    return picture
}

private fun embeddedPicture(context: Context, coverUri: String): ByteArray? {
    if (coverUri.isEmpty()) return null
    val retriever = MediaMetadataRetriever()
    return try {
        if (coverUri.startsWith("content://")) {
            retriever.setDataSource(context, coverUri.toUri())
        } else {
            retriever.setDataSource(coverUri)
        }
        retriever.embeddedPicture
    } catch (t: Throwable) {
        null
    } finally {
        runCatching { retriever.release() }
    }
}
