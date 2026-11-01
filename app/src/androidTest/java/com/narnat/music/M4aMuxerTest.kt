package com.narnat.music

import android.media.MediaExtractor
import android.media.MediaFormat
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.narnat.music.data.audio.M4aMuxer
import java.io.File
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * `M4aMuxer` 依赖真实 MediaExtractor/MediaMuxer，只能在设备/模拟器上跑。
 * 样本是 `docs/probe/sample_BV1aB4y1u7ox.m4a`（B 站 fMP4，36 个 moof/mdat 分片）。
 */
@RunWith(AndroidJUnit4::class)
class M4aMuxerTest {

    @Test
    fun remuxesFragmentedFmp4ToPlainMp4() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val dir = File(instrumentation.targetContext.cacheDir, "muxer-test").apply { mkdirs() }
        val source = File(dir, "sample_BV1aB4y1u7ox.m4a")
        instrumentation.context.assets.open(SAMPLE_ASSET).use { input ->
            source.outputStream().use { output -> input.copyTo(output) }
        }
        assertTrue("样本必须复制成功", source.length() > 1_000_000L)

        val sourceDurationUs = audioDurationUs(source)
        assertTrue(sourceDurationUs > 0L)

        val target = File(dir, "remuxed.m4a")
        assertTrue("remux 必须成功", M4aMuxer.remux(source, target))
        assertTrue(target.length() > 0L)

        val extractor = MediaExtractor()
        try {
            extractor.setDataSource(target.absolutePath)
            assertEquals("输出必须只有一条音频轨", 1, extractor.trackCount)
            val format = extractor.getTrackFormat(0)
            val mime = format.getString(MediaFormat.KEY_MIME)
            assertNotNull(mime)
            assertTrue("MIME 必须以 audio/ 开头，实际=$mime", mime!!.startsWith("audio/"))
            val durationUs = format.getLong(MediaFormat.KEY_DURATION)
            assertTrue(
                "时长偏差必须小于 100ms（源=$sourceDurationUs, 输出=$durationUs）",
                Math.abs(sourceDurationUs - durationUs) < 100_000L
            )
        } finally {
            extractor.release()
        }
    }

    private fun audioDurationUs(file: File): Long {
        val extractor = MediaExtractor()
        try {
            extractor.setDataSource(file.absolutePath)
            for (i in 0 until extractor.trackCount) {
                val format = extractor.getTrackFormat(i)
                val mime = format.getString(MediaFormat.KEY_MIME) ?: continue
                if (mime.startsWith("audio/") && format.containsKey(MediaFormat.KEY_DURATION)) {
                    return format.getLong(MediaFormat.KEY_DURATION)
                }
            }
        } finally {
            extractor.release()
        }
        return 0L
    }

    companion object {
        private const val SAMPLE_ASSET = "sample_BV1aB4y1u7ox.m4a"
    }
}
