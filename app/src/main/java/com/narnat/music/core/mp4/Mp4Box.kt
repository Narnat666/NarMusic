package com.narnat.music.core.mp4

import java.io.File
import java.io.RandomAccessFile

/**
 * 单个 MP4 box 的头信息。offset/size 覆盖 box 头部本身。
 */
data class Mp4BoxInfo(
    val type: String,
    val offset: Long,
    val size: Long,
    val headerSize: Int
) {
    val payloadOffset: Long get() = offset + headerSize
    val payloadSize: Long get() = size - headerSize
    val end: Long get() = offset + size
}

/**
 * MP4 box 结构的顺序解析器，纯 JVM 实现（不依赖 Android API）。
 */
object Mp4Box {

    const val HEADER_SIZE = 8
    const val LARGE_HEADER_SIZE = 16

    /** `meta` 是 fullbox，子 box 从 header 之后多跳 4 字节（version/flags）。 */
    const val META_HEADER_SKIP = 4

    const val TYPE_FTYP = "ftyp"
    const val TYPE_FREE = "free"
    const val TYPE_MOOV = "moov"
    const val TYPE_MDAT = "mdat"
    const val TYPE_UDTA = "udta"
    const val TYPE_META = "meta"
    const val TYPE_HLDR = "hdlr"
    const val TYPE_ILST = "ilst"
    const val TYPE_MOOF = "moof"
    const val TYPE_TRAF = "traf"
    const val TYPE_TFHD = "tfhd"

    /** 可以安全递归下钻的容器 box（不含 fullbox，避免多跳 4 字节）。 */
    val CONTAINER_TYPES: Set<String> = setOf(
        "moov", "trak", "mdia", "minf", "stbl", "udta", "edts", "dinf", "mvex", "moof", "traf", "mfra"
    )

    /** 顺序解析整个文件的顶层 box。解析到非法/截断处即停止，不抛异常。 */
    fun parse(file: File): List<Mp4BoxInfo> {
        val length = file.length()
        if (length < HEADER_SIZE) return emptyList()
        val result = ArrayList<Mp4BoxInfo>()
        RandomAccessFile(file, "r").use { raf ->
            val buf = ByteArray(LARGE_HEADER_SIZE)
            var pos = 0L
            while (pos + HEADER_SIZE <= length) {
                raf.seek(pos)
                if (raf.read(buf, 0, HEADER_SIZE) < HEADER_SIZE) break
                val size32 = readUInt32(buf, 0)
                val type = fourCC(buf, 4)
                var size = size32
                var headerSize = HEADER_SIZE
                if (size32 == 1L) {
                    if (pos + LARGE_HEADER_SIZE > length) break
                    if (raf.read(buf, HEADER_SIZE, HEADER_SIZE) < HEADER_SIZE) break
                    size = readUInt64(buf, 8)
                    headerSize = LARGE_HEADER_SIZE
                } else if (size32 == 0L) {
                    size = length - pos
                }
                if (size < headerSize || pos + size > length) break
                result.add(Mp4BoxInfo(type, pos, size, headerSize))
                pos += size
            }
        }
        return result
    }

    /** 顺序解析内存字节区间内的 box。offset 为相对字节数组起点的绝对下标。 */
    fun parse(bytes: ByteArray, start: Int = 0, end: Int = bytes.size): List<Mp4BoxInfo> {
        val result = ArrayList<Mp4BoxInfo>()
        var pos = start
        while (pos + HEADER_SIZE <= end) {
            val size32 = readUInt32(bytes, pos)
            val type = fourCC(bytes, pos + 4)
            var size = size32
            var headerSize = HEADER_SIZE
            if (size32 == 1L) {
                if (pos + LARGE_HEADER_SIZE > end) break
                size = readUInt64(bytes, pos + 8)
                headerSize = LARGE_HEADER_SIZE
            } else if (size32 == 0L) {
                size = (end - pos).toLong()
            }
            if (size < headerSize || pos + size > end) break
            result.add(Mp4BoxInfo(type, pos.toLong(), size, headerSize))
            pos += size.toInt()
        }
        return result
    }

    fun readUInt32(bytes: ByteArray, offset: Int): Long =
        ((bytes[offset].toLong() and 0xFF) shl 24) or
            ((bytes[offset + 1].toLong() and 0xFF) shl 16) or
            ((bytes[offset + 2].toLong() and 0xFF) shl 8) or
            (bytes[offset + 3].toLong() and 0xFF)

    fun readUInt64(bytes: ByteArray, offset: Int): Long =
        (readUInt32(bytes, offset) shl 32) or readUInt32(bytes, offset + 4)

    fun writeUInt32(bytes: ByteArray, offset: Int, value: Long) {
        bytes[offset] = ((value ushr 24) and 0xFF).toByte()
        bytes[offset + 1] = ((value ushr 16) and 0xFF).toByte()
        bytes[offset + 2] = ((value ushr 8) and 0xFF).toByte()
        bytes[offset + 3] = (value and 0xFF).toByte()
    }

    fun writeUInt64(bytes: ByteArray, offset: Int, value: Long) {
        writeUInt32(bytes, offset, value ushr 32)
        writeUInt32(bytes, offset + 4, value and 0xFFFFFFFFL)
    }

    /** 用 ISO-8859-1 做 1:1 字节映射解码 4 字节 type（保留 `©` 这类 0xA9 字节）。 */
    fun fourCC(bytes: ByteArray, offset: Int): String =
        String(bytes, offset, 4, Charsets.ISO_8859_1)

    fun typeBytes(type: String): ByteArray = type.toByteArray(Charsets.ISO_8859_1)

    /** 构造一个普通 box（header 8 字节 + payload）。 */
    fun buildBox(type: String, payload: ByteArray): ByteArray {
        val out = ByteArray(payload.size + HEADER_SIZE)
        writeUInt32(out, 0, (payload.size + HEADER_SIZE).toLong())
        typeBytes(type).copyInto(out, 4)
        payload.copyInto(out, HEADER_SIZE)
        return out
    }

    /**
     * 以原始 box 字节为基础重建 box：保留 header 的 type、version/flags 段，替换 children 字节。
     * @param original 完整的原 box 字节（含 header）
     * @param headerSize 原 box 的 header 长度（8 或 16）
     * @param extraSkip header 之后额外跳过的字节数（fullbox 为 4）
     * @param body 新的 children 字节（从 headerSize + extraSkip 开始）
     */
    fun rebuildBox(original: ByteArray, headerSize: Int, extraSkip: Int, body: ByteArray): ByteArray {
        val dataStart = headerSize + extraSkip
        val total = dataStart + body.size
        val out = ByteArray(total)
        if (headerSize == LARGE_HEADER_SIZE) {
            writeUInt32(out, 0, 1L)
            original.copyInto(out, 4, 4, 8)
            writeUInt64(out, 8, total.toLong())
        } else {
            writeUInt32(out, 0, total.toLong())
            original.copyInto(out, 4, 4, 8)
        }
        if (extraSkip > 0) original.copyInto(out, headerSize, headerSize, dataStart)
        body.copyInto(out, dataStart)
        return out
    }

    fun slice(bytes: ByteArray, box: Mp4BoxInfo): ByteArray =
        bytes.copyOfRange(box.offset.toInt(), box.end.toInt())
}
