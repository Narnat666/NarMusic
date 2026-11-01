package com.narnat.music.data.audio

import android.media.MediaCodec
import android.media.MediaExtractor
import android.media.MediaFormat
import android.media.MediaMuxer
import java.io.File
import java.nio.ByteBuffer

/**
 * 用 [MediaExtractor] + [MediaMuxer] 把 B 站下载到的 fMP4（`ftyp/moov/sidx/(moof/mdat)×N`）
 * 重封装成标准 MP4（moov 在文件末尾、单音频轨）。失败返回 false，由调用方退回直接使用原文件。
 */
object M4aMuxer {

    private const val FALLBACK_BUFFER_SIZE = 256 * 1024

    fun remux(source: File, target: File): Boolean {
        if (!source.isFile || source.length() <= 0L) return false
        var extractor: MediaExtractor? = null
        var muxer: MediaMuxer? = null
        var started = false
        try {
            extractor = MediaExtractor()
            extractor.setDataSource(source.absolutePath)
            var trackIndex = -1
            var format: MediaFormat? = null
            for (i in 0 until extractor.trackCount) {
                val candidate = extractor.getTrackFormat(i)
                val mime = candidate.getString(MediaFormat.KEY_MIME) ?: continue
                if (mime.startsWith("audio/")) {
                    trackIndex = i
                    format = candidate
                    break
                }
            }
            if (trackIndex < 0 || format == null) return false

            extractor.selectTrack(trackIndex)
            target.parentFile?.mkdirs()
            if (target.exists() && !target.delete()) return false

            muxer = MediaMuxer(target.absolutePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
            val outTrack = muxer.addTrack(format)
            muxer.start()
            started = true

            val declared = if (format.containsKey(MediaFormat.KEY_MAX_INPUT_SIZE)) {
                format.getInteger(MediaFormat.KEY_MAX_INPUT_SIZE)
            } else {
                0
            }
            val bufferSize = if (declared > 0) declared else FALLBACK_BUFFER_SIZE
            val buffer = ByteBuffer.allocateDirect(maxOf(bufferSize, FALLBACK_BUFFER_SIZE))
            val info = MediaCodec.BufferInfo()
            while (true) {
                buffer.clear()
                val sampleSize = extractor.readSampleData(buffer, 0)
                if (sampleSize < 0) break
                info.offset = 0
                info.size = sampleSize
                info.presentationTimeUs = extractor.sampleTime
                info.flags = toBufferFlags(extractor.sampleFlags)
                muxer.writeSampleData(outTrack, buffer, info)
                if (!extractor.advance()) break
            }
            muxer.stop()
            started = false
            muxer.release()
            muxer = null
            return target.isFile && target.length() > 0L
        } catch (t: Throwable) {
            if (started) {
                try {
                    muxer?.stop()
                } catch (ignored: Throwable) {
                    // stop 失败时 release 仍要执行
                }
            }
            try {
                muxer?.release()
            } catch (ignored: Throwable) {
                // 释放失败无需向上传播
            }
            muxer = null
            target.delete()
            return false
        } finally {
            extractor?.release()
        }
    }

    /** `MediaExtractor.sampleFlags` 与 `MediaCodec.BufferInfo.flags` 是两套常量，显式映射。 */
    private fun toBufferFlags(sampleFlags: Int): Int {
        var flags = 0
        if ((sampleFlags and MediaExtractor.SAMPLE_FLAG_SYNC) != 0) {
            flags = flags or MediaCodec.BUFFER_FLAG_KEY_FRAME
        }
        if ((sampleFlags and MediaExtractor.SAMPLE_FLAG_PARTIAL_FRAME) != 0) {
            flags = flags or MediaCodec.BUFFER_FLAG_PARTIAL_FRAME
        }
        return flags
    }
}
