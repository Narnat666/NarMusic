package com.narnat.music.ui.components

import android.content.Context
import android.media.MediaMetadataRetriever
import android.util.Log
import android.util.LruCache
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.produceState
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.core.net.toUri
import coil.compose.AsyncImage
import com.narnat.music.ui.theme.NarCoverThumb
import com.narnat.music.ui.theme.NarThumbShape
import java.util.Collections
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import kotlinx.coroutines.withContext

private const val TAG = "NarCover"
private const val MAX_ATTEMPTS = 3
private const val MAX_TOTAL_ATTEMPTS = 6

/** 串行限流：多个 MediaMetadataRetriever 并发打开同一批文件会互相踢掉句柄 */
private val coverGate = Semaphore(1)

/**
 * 行首封面缓存：成功解码的字节进 LruCache；
 * 读取成功但没有内嵌封面记为「确定无封面」；读取抛异常只累加重试次数，不写死。
 */
private object NarCoverCache {
    private val pictures = LruCache<String, ByteArray>(64)
    private val noPicture: MutableSet<String> = Collections.synchronizedSet(HashSet())
    private val failures: MutableMap<String, Int> = Collections.synchronizedMap(HashMap())

    fun picture(key: String): ByteArray? = synchronized(pictures) { pictures.get(key) }

    fun isSettledMiss(key: String): Boolean = noPicture.contains(key)

    fun putPicture(key: String, bytes: ByteArray) {
        failures.remove(key)
        noPicture.remove(key)
        synchronized(pictures) { pictures.put(key, bytes) }
    }

    fun putNoPicture(key: String) {
        failures.remove(key)
        noPicture.add(key)
    }

    fun putFailure(key: String) {
        failures[key] = (failures[key] ?: 0) + 1
        if ((failures[key] ?: 0) >= MAX_TOTAL_ATTEMPTS) {
            failures.remove(key)
            noPicture.add(key)
            Log.w(TAG, "封面多次读取失败，降级占位 key=$key")
        }
    }
}

/** 读取内嵌封面：返回 null 表示读取成功但文件本身没有封面；抛异常表示本次读取失败 */
private fun readEmbeddedPicture(context: Context, source: String): ByteArray? {
    if (source.isEmpty()) return null
    val retriever = MediaMetadataRetriever()
    return try {
        if (source.startsWith("content://")) {
            retriever.setDataSource(context, source.toUri())
        } else {
            retriever.setDataSource(source)
        }
        retriever.embeddedPicture
    } finally {
        runCatching { retriever.release() }
    }
}

private suspend fun loadCover(context: Context, source: String): ByteArray? {
    repeat(MAX_ATTEMPTS) { attempt ->
        val outcome = coverGate.withPermit {
            withContext(Dispatchers.IO) { runCatching { readEmbeddedPicture(context, source) } }
        }
        val bytes = outcome.getOrNull()
        if (bytes != null && bytes.isNotEmpty()) {
            NarCoverCache.putPicture(source, bytes)
            Log.d(TAG, "封面读取成功 key=$source size=${bytes.size}")
            return bytes
        }
        if (outcome.isSuccess) {
            NarCoverCache.putNoPicture(source)
            Log.d(TAG, "文件无内嵌封面 key=$source")
            return null
        }
        NarCoverCache.putFailure(source)
        Log.w(
            TAG,
            "封面读取失败第${attempt + 1}次 key=$source: ${outcome.exceptionOrNull()?.message}"
        )
        delay(60L * (attempt + 1))
    }
    return null
}

/** 列表行首专辑封面（48dp / 圆角 12dp）；读不到内嵌封面时降级为主色浅块 + 音符图标 */
@Composable
fun NarAlbumCover(source: String, modifier: Modifier = Modifier, size: Dp = 48.dp) {
    val context = LocalContext.current
    val initial = if (source.isEmpty()) null else NarCoverCache.picture(source)
    val picture by produceState<ByteArray?>(initialValue = initial, source) {
        value = when {
            source.isEmpty() -> null
            NarCoverCache.isSettledMiss(source) -> null
            initial != null -> initial
            else -> loadCover(context, source)
        }
    }
    if (picture != null) {
        AsyncImage(
            model = picture,
            contentDescription = null,
            contentScale = ContentScale.Crop,
            modifier = modifier
                .size(size)
                .clip(NarThumbShape)
        )
    } else {
        NarCoverThumb(modifier = modifier, size = size) {
            Icon(
                imageVector = NarIcons.LibraryMusic,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.primary.copy(alpha = 0.55f),
                modifier = Modifier.size(size * 0.42f)
            )
        }
    }
}
