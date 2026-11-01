package com.narnat.music.core.mp4

import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.OutputStream
import java.io.RandomAccessFile
import java.security.MessageDigest

/**
 * 要写入 M4A 的标签集合。空字符串/空数组表示「删除该 atom」。
 */
data class Mp4Tags(
    val title: String = "",
    val artist: String = "",
    val album: String = "",
    val lyrics: String = "",
    val cover: ByteArray? = null,
    val delayMs: Int = 0,
    val narmeta: String = ""
) {
    fun coverEquals(other: ByteArray?): Boolean {
        val a = cover
        if (a == null || a.isEmpty()) return other == null || other.isEmpty()
        return other != null && a.contentEquals(other)
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is Mp4Tags) return false
        return title == other.title && artist == other.artist && album == other.album &&
            lyrics == other.lyrics && delayMs == other.delayMs && narmeta == other.narmeta &&
            coverEquals(other.cover)
    }

    override fun hashCode(): Int {
        var result = title.hashCode()
        result = 31 * result + artist.hashCode()
        result = 31 * result + album.hashCode()
        result = 31 * result + lyrics.hashCode()
        result = 31 * result + delayMs
        result = 31 * result + narmeta.hashCode()
        result = 31 * result + (cover?.contentHashCode() ?: 0)
        return result
    }
}

/**
 * 把 UTF-8 标签、封面与自定义 narmeta 原子写进 MP4 的 moov.udta.meta.ilst。
 *
 * 偏移安全策略（不重算 chunk offset，除「moov 变大」那一个分支）：
 * 1. moov 是最后一个顶层 box → 截断到 moov.offset，追加新 moov。
 * 2. moov 不在末尾且新 moov 更小/相等 → 新 moov + free 补齐，后面字节位置不变。
 * 3. moov 不在末尾且新 moov 更大 → 后续字节整体后移 delta，修正 stco/co64 与 tfhd 的
 *    base_data_offset；sidx 全是相对锚点，不改。
 */
object Mp4TagWriter {

    const val FREEFORM_MEAN = "com.narnat"
    const val FREEFORM_DELAY = "delayMs"
    const val FREEFORM_NARMETA = "narmeta"

    private const val TAG_TITLE = "\u00A9nam"
    private const val TAG_ARTIST = "\u00A9ART"
    private const val TAG_ALBUM = "\u00A9alb"
    private const val TAG_LYRICS = "\u00A9lyr"
    private const val TAG_COVER = "covr"
    private const val TAG_FREEFORM = "----"

    private val MANAGED_TEXT_TAGS = setOf(TAG_TITLE, TAG_ARTIST, TAG_ALBUM, TAG_LYRICS)

    private const val MAX_MOOV_BYTES = 64L * 1024 * 1024
    private const val COPY_BUFFER_SIZE = 64 * 1024

    fun write(file: File, tags: Mp4Tags): Boolean {
        if (!file.isFile || file.length() < Mp4Box.HEADER_SIZE) return false
        val top = try {
            Mp4Box.parse(file)
        } catch (e: IOException) {
            return false
        }
        val moov = top.firstOrNull { it.type == Mp4Box.TYPE_MOOV } ?: return false
        if (moov.size > MAX_MOOV_BYTES || moov.size < moov.headerSize) return false

        val moovBytes = (try {
            readRange(file, moov.offset, moov.size.toInt())
        } catch (e: IOException) {
            null
        }) ?: return false
        val newMoov = try {
            rebuildMoov(moovBytes, moov.headerSize, tags)
        } catch (e: IOException) {
            null
        } ?: return false
        if (newMoov.contentEquals(moovBytes)) return true

        val dir = file.parentFile ?: return false
        val tmp = File(dir, file.name + ".mp4tag.tmp")
        if (tmp.exists() && !tmp.delete()) return false
        val ok = try {
            writeRebuiltFile(tmp, file, moov, newMoov) && verify(tmp, file, tags) && replace(tmp, file)
        } catch (t: Throwable) {
            false
        }
        if (!ok) tmp.delete()
        return ok
    }

    // ---------- 新文件写入 ----------

    private fun writeRebuiltFile(tmp: File, src: File, moov: Mp4BoxInfo, newMoovIn: ByteArray): Boolean {
        val srcLength = src.length()
        if (moov.end == srcLength) {
            FileOutputStream(tmp).use { out ->
                if (!copyRange(src, 0L, moov.offset, out)) return false
                out.write(newMoovIn)
            }
            return true
        }

        var newMoov = newMoovIn
        var delta = newMoov.size.toLong() - moov.size
        if (delta < 0 && -delta < Mp4Box.HEADER_SIZE) {
            newMoov = appendFreeInsideBox(newMoov, moov.headerSize, Mp4Box.HEADER_SIZE)
            delta = newMoov.size.toLong() - moov.size
        }

        if (delta <= 0) {
            val pad = (-delta).toInt()
            FileOutputStream(tmp).use { out ->
                if (!copyRange(src, 0L, moov.offset, out)) return false
                out.write(newMoov)
                if (pad > 0) out.write(buildFreeBox(pad))
                if (!copyRange(src, moov.end, srcLength, out)) return false
            }
            return true
        }

        val shifted = shiftChunkOffsets(newMoov, delta) ?: return false
        val tail = (try {
            readRange(src, moov.end, (srcLength - moov.end).toInt())
        } catch (e: IOException) {
            null
        }) ?: return false
        val patches = ArrayList<Long>()
        try {
            RandomAccessFile(src, "r").use { raf -> scanTfhdBaseOffsets(raf, moov.end, srcLength, patches) }
        } catch (e: IOException) {
            return false
        }
        for (p in patches) {
            val local = (p - moov.end).toInt()
            if (local < 0 || local + 8 > tail.size) return false
            val v = Mp4Box.readUInt64(tail, local) + delta
            if (v < 0) return false
            Mp4Box.writeUInt64(tail, local, v)
        }
        FileOutputStream(tmp).use { out ->
            if (!copyRange(src, 0L, moov.offset, out)) return false
            out.write(shifted)
            out.write(tail)
        }
        return true
    }

    private fun readRange(file: File, offset: Long, length: Int): ByteArray? {
        if (length < 0) return null
        val buf = ByteArray(length)
        RandomAccessFile(file, "r").use { raf ->
            raf.seek(offset)
            var read = 0
            while (read < length) {
                val n = raf.read(buf, read, length - read)
                if (n <= 0) return null
                read += n
            }
        }
        return buf
    }

    private fun copyRange(src: File, from: Long, to: Long, out: OutputStream): Boolean {
        if (to <= from) return true
        val buf = ByteArray(COPY_BUFFER_SIZE)
        RandomAccessFile(src, "r").use { raf ->
            raf.seek(from)
            var remaining = to - from
            while (remaining > 0) {
                val n = raf.read(buf, 0, minOf(buf.size.toLong(), remaining).toInt())
                if (n <= 0) return false
                out.write(buf, 0, n)
                remaining -= n
            }
        }
        return true
    }

    private fun buildFreeBox(size: Int): ByteArray {
        val actual = if (size < Mp4Box.HEADER_SIZE) Mp4Box.HEADER_SIZE else size
        return Mp4Box.buildBox(Mp4Box.TYPE_FREE, ByteArray(actual - Mp4Box.HEADER_SIZE))
    }

    private fun appendFreeInsideBox(box: ByteArray, headerSize: Int, extra: Int): ByteArray {
        val payload = box.copyOfRange(headerSize, box.size)
        val body = ByteArrayOutputStream()
        body.write(payload)
        body.write(buildFreeBox(extra))
        return Mp4Box.rebuildBox(box, headerSize, 0, body.toByteArray())
    }

    private fun scanTfhdBaseOffsets(raf: RandomAccessFile, start: Long, end: Long, out: MutableList<Long>) {
        val buf = ByteArray(16)
        var pos = start
        while (pos + Mp4Box.HEADER_SIZE <= end) {
            raf.seek(pos)
            if (raf.read(buf, 0, Mp4Box.HEADER_SIZE) < Mp4Box.HEADER_SIZE) return
            var size = Mp4Box.readUInt32(buf, 0)
            val type = Mp4Box.fourCC(buf, 4)
            var headerSize = Mp4Box.HEADER_SIZE
            if (size == 1L) {
                if (pos + Mp4Box.LARGE_HEADER_SIZE > end) return
                if (raf.read(buf, Mp4Box.HEADER_SIZE, Mp4Box.HEADER_SIZE) < Mp4Box.HEADER_SIZE) return
                size = Mp4Box.readUInt64(buf, 8)
                headerSize = Mp4Box.LARGE_HEADER_SIZE
            } else if (size == 0L) {
                size = end - pos
            }
            if (size < headerSize || pos + size > end) return
            when (type) {
                Mp4Box.TYPE_MOOF, Mp4Box.TYPE_TRAF -> scanTfhdBaseOffsets(raf, pos + headerSize, pos + size, out)
                Mp4Box.TYPE_TFHD -> {
                    if (size >= headerSize + 8) {
                        raf.seek(pos + headerSize)
                        if (raf.read(buf, 0, 8) == 8) {
                            val flags = Mp4Box.readUInt32(buf, 0) and 0xFFFFFFL
                            if ((flags and 0x01L) != 0L) {
                                val p = pos + headerSize + 8
                                if (p + 8 <= pos + size) out.add(p)
                            }
                        }
                    }
                }
            }
            pos += size
        }
    }

    // ---------- stco / co64 修正 ----------

    private fun shiftChunkOffsets(box: ByteArray, delta: Long): ByteArray? {
        val copy = box.copyOf()
        return if (patchChunkOffsets(copy, 0, copy.size, delta)) copy else null
    }

    private fun patchChunkOffsets(bytes: ByteArray, start: Int, end: Int, delta: Long): Boolean {
        val children = Mp4Box.parse(bytes, start, end)
        for (c in children) {
            val o = c.offset.toInt()
            when {
                c.type == "stco" -> if (!patchStco(bytes, o, c, delta)) return false
                c.type == "co64" -> if (!patchCo64(bytes, o, c, delta)) return false
                c.type in Mp4Box.CONTAINER_TYPES ->
                    if (!patchChunkOffsets(bytes, o + c.headerSize, o + c.size.toInt(), delta)) return false
            }
        }
        return true
    }

    private fun patchStco(bytes: ByteArray, offset: Int, box: Mp4BoxInfo, delta: Long): Boolean {
        val base = offset + box.headerSize
        if (base + 8 > offset + box.size) return false
        val count = Mp4Box.readUInt32(bytes, base + 4)
        if (count < 0 || count > Int.MAX_VALUE / 4) return false
        if (base + 8 + count * 4 > offset + box.size) return false
        for (i in 0 until count.toInt()) {
            val p = base + 8 + i * 4
            val v = Mp4Box.readUInt32(bytes, p) + delta
            if (v < 0 || v > 0xFFFFFFFFL) return false
            Mp4Box.writeUInt32(bytes, p, v)
        }
        return true
    }

    private fun patchCo64(bytes: ByteArray, offset: Int, box: Mp4BoxInfo, delta: Long): Boolean {
        val base = offset + box.headerSize
        if (base + 8 > offset + box.size) return false
        val count = Mp4Box.readUInt32(bytes, base + 4)
        if (count < 0 || count > Int.MAX_VALUE / 8) return false
        if (base + 8 + count * 8 > offset + box.size) return false
        for (i in 0 until count.toInt()) {
            val p = base + 8 + i * 8
            val v = Mp4Box.readUInt64(bytes, p) + delta
            if (v < 0) return false
            Mp4Box.writeUInt64(bytes, p, v)
        }
        return true
    }

    // ---------- moov 重建 ----------

    private fun rebuildMoov(moovBytes: ByteArray, headerSize: Int, tags: Mp4Tags): ByteArray? {
        val moovChildren = Mp4Box.parse(moovBytes, headerSize, moovBytes.size)
        val udtaInfo = moovChildren.firstOrNull { it.type == Mp4Box.TYPE_UDTA }
        val existingIlst = findIlst(moovBytes, udtaInfo)
        val newIlst = buildIlst(existingIlst, tags)

        val newUdta: ByteArray
        if (udtaInfo == null) {
            newUdta = Mp4Box.buildBox(Mp4Box.TYPE_UDTA, buildMeta(null, newIlst))
        } else {
            val udtaBytes = Mp4Box.slice(moovBytes, udtaInfo)
            val udtaChildren = Mp4Box.parse(udtaBytes, udtaInfo.headerSize, udtaBytes.size)
            val metaInfo = udtaChildren.firstOrNull { it.type == Mp4Box.TYPE_META }
            val newMeta = if (metaInfo == null) {
                buildMeta(null, newIlst)
            } else {
                buildMeta(Mp4Box.slice(udtaBytes, metaInfo), newIlst)
            }
            val body = ByteArrayOutputStream()
            var replaced = false
            for (c in udtaChildren) {
                if (c.type == Mp4Box.TYPE_META) {
                    body.write(newMeta)
                    replaced = true
                } else {
                    body.write(udtaBytes, c.offset.toInt(), c.size.toInt())
                }
            }
            if (!replaced) body.write(newMeta)
            newUdta = Mp4Box.rebuildBox(udtaBytes, udtaInfo.headerSize, 0, body.toByteArray())
        }

        val moovBody = ByteArrayOutputStream()
        var replaced = false
        for (c in moovChildren) {
            if (c.type == Mp4Box.TYPE_UDTA) {
                moovBody.write(newUdta)
                replaced = true
            } else {
                moovBody.write(moovBytes, c.offset.toInt(), c.size.toInt())
            }
        }
        if (!replaced) moovBody.write(newUdta)
        return Mp4Box.rebuildBox(moovBytes, headerSize, 0, moovBody.toByteArray())
    }

    private fun findIlst(moovBytes: ByteArray, udtaInfo: Mp4BoxInfo?): ByteArray? {
        if (udtaInfo == null) return null
        val udtaBytes = Mp4Box.slice(moovBytes, udtaInfo)
        val metaInfo = Mp4Box.parse(udtaBytes, udtaInfo.headerSize, udtaBytes.size)
            .firstOrNull { it.type == Mp4Box.TYPE_META } ?: return null
        val metaBytes = Mp4Box.slice(udtaBytes, metaInfo)
        val ilstInfo = Mp4Box.parse(metaBytes, metaInfo.headerSize + Mp4Box.META_HEADER_SKIP, metaBytes.size)
            .firstOrNull { it.type == Mp4Box.TYPE_ILST } ?: return null
        return Mp4Box.slice(metaBytes, ilstInfo)
    }

    private fun buildMeta(existing: ByteArray?, ilst: ByteArray): ByteArray {
        if (existing == null) {
            val payload = ByteArrayOutputStream()
            payload.write(byteArrayOf(0, 0, 0, 0))
            payload.write(buildHdlr())
            payload.write(ilst)
            return Mp4Box.buildBox(Mp4Box.TYPE_META, payload.toByteArray())
        }
        val headerSize = Mp4Box.HEADER_SIZE
        val children = Mp4Box.parse(existing, headerSize + Mp4Box.META_HEADER_SKIP, existing.size)
        val body = ByteArrayOutputStream()
        if (children.none { it.type == Mp4Box.TYPE_HLDR }) body.write(buildHdlr())
        var replaced = false
        for (c in children) {
            if (c.type == Mp4Box.TYPE_ILST) {
                body.write(ilst)
                replaced = true
            } else {
                body.write(existing, c.offset.toInt(), c.size.toInt())
            }
        }
        if (!replaced) body.write(ilst)
        return Mp4Box.rebuildBox(existing, headerSize, Mp4Box.META_HEADER_SKIP, body.toByteArray())
    }

    private fun buildHdlr(): ByteArray {
        val payload = ByteArray(25)
        Mp4Box.typeBytes("mdir").copyInto(payload, 8)
        return Mp4Box.buildBox(Mp4Box.TYPE_HLDR, payload)
    }

    private fun buildIlst(existing: ByteArray?, tags: Mp4Tags): ByteArray {
        val body = ByteArrayOutputStream()
        if (existing != null) {
            val headerSize = Mp4Box.HEADER_SIZE
            for (c in Mp4Box.parse(existing, headerSize, existing.size)) {
                if (isManagedAtom(existing, c)) continue
                body.write(existing, c.offset.toInt(), c.size.toInt())
            }
        }
        if (tags.title.isNotEmpty()) body.write(buildTextAtom(TAG_TITLE, tags.title))
        if (tags.artist.isNotEmpty()) body.write(buildTextAtom(TAG_ARTIST, tags.artist))
        if (tags.album.isNotEmpty()) body.write(buildTextAtom(TAG_ALBUM, tags.album))
        if (tags.lyrics.isNotEmpty()) body.write(buildTextAtom(TAG_LYRICS, tags.lyrics))
        val cover = tags.cover
        if (cover != null && cover.isNotEmpty()) body.write(buildCoverAtom(cover))
        if (tags.delayMs != 0) body.write(buildFreeformAtom(FREEFORM_DELAY, tags.delayMs.toString()))
        if (tags.narmeta.isNotEmpty()) body.write(buildFreeformAtom(FREEFORM_NARMETA, tags.narmeta))
        return Mp4Box.buildBox(Mp4Box.TYPE_ILST, body.toByteArray())
    }

    private fun isManagedAtom(container: ByteArray, box: Mp4BoxInfo): Boolean {
        if (box.type in MANAGED_TEXT_TAGS || box.type == TAG_COVER) return true
        if (box.type != TAG_FREEFORM) return false
        val children = Mp4Box.parse(container, box.offset.toInt() + box.headerSize, box.end.toInt())
        val mean = children.firstOrNull { it.type == "mean" }?.let { readFullBoxText(container, it) }
        val name = children.firstOrNull { it.type == "name" }?.let { readFullBoxText(container, it) }
        return mean == FREEFORM_MEAN && (name == FREEFORM_DELAY || name == FREEFORM_NARMETA)
    }

    private fun readFullBoxText(container: ByteArray, box: Mp4BoxInfo): String {
        val start = box.offset.toInt() + box.headerSize + 4
        val end = box.end.toInt()
        if (start >= end) return ""
        return String(container, start, end - start, Charsets.UTF_8)
    }

    private fun buildTextAtom(type: String, text: String): ByteArray {
        val content = text.toByteArray(Charsets.UTF_8)
        val dataPayload = ByteArray(8 + content.size)
        Mp4Box.writeUInt32(dataPayload, 0, 0x01L)
        content.copyInto(dataPayload, 8)
        return Mp4Box.buildBox(type, Mp4Box.buildBox("data", dataPayload))
    }

    private fun buildCoverAtom(cover: ByteArray): ByteArray {
        val flags = if (isPng(cover)) 0x0EL else 0x0DL
        val dataPayload = ByteArray(8 + cover.size)
        Mp4Box.writeUInt32(dataPayload, 0, flags)
        cover.copyInto(dataPayload, 8)
        return Mp4Box.buildBox(TAG_COVER, Mp4Box.buildBox("data", dataPayload))
    }

    private fun buildFreeformAtom(name: String, value: String): ByteArray {
        val meanPayload = ByteArray(4 + FREEFORM_MEAN.length)
        FREEFORM_MEAN.toByteArray(Charsets.UTF_8).copyInto(meanPayload, 4)
        val nameBytes = name.toByteArray(Charsets.UTF_8)
        val namePayload = ByteArray(4 + nameBytes.size)
        nameBytes.copyInto(namePayload, 4)
        val valueBytes = value.toByteArray(Charsets.UTF_8)
        val dataPayload = ByteArray(8 + valueBytes.size)
        Mp4Box.writeUInt32(dataPayload, 0, 0x01L)
        valueBytes.copyInto(dataPayload, 8)
        val body = ByteArrayOutputStream()
        body.write(Mp4Box.buildBox("mean", meanPayload))
        body.write(Mp4Box.buildBox("name", namePayload))
        body.write(Mp4Box.buildBox("data", dataPayload))
        return Mp4Box.buildBox(TAG_FREEFORM, body.toByteArray())
    }

    private fun isPng(bytes: ByteArray): Boolean =
        bytes.size > 4 && (bytes[0].toInt() and 0xFF) == 0x89 &&
            bytes[1].toInt() == 0x50 && bytes[2].toInt() == 0x4E && bytes[3].toInt() == 0x47

    // ---------- 校验与替换 ----------

    private fun verify(tmp: File, src: File, tags: Mp4Tags): Boolean {
        val read = Mp4TagReader.read(tmp)
        if (read.title != tags.title) return false
        if (read.artist != tags.artist) return false
        if (read.album != tags.album) return false
        if (read.lyrics != tags.lyrics) return false
        if (read.narmeta != tags.narmeta) return false
        if (read.delayMs != tags.delayMs) return false
        if (!tags.coverEquals(read.cover)) return false
        return mdatDigest(tmp) == mdatDigest(src)
    }

    private fun mdatDigest(file: File): String {
        val md = MessageDigest.getInstance("MD5")
        val length = file.length()
        val buf = ByteArray(COPY_BUFFER_SIZE)
        RandomAccessFile(file, "r").use { raf ->
            var pos = 0L
            while (pos + Mp4Box.HEADER_SIZE <= length) {
                raf.seek(pos)
                if (raf.read(buf, 0, Mp4Box.HEADER_SIZE) < Mp4Box.HEADER_SIZE) break
                var size = Mp4Box.readUInt32(buf, 0)
                val type = Mp4Box.fourCC(buf, 4)
                var headerSize = Mp4Box.HEADER_SIZE
                if (size == 1L) {
                    if (raf.read(buf, Mp4Box.HEADER_SIZE, Mp4Box.HEADER_SIZE) < Mp4Box.HEADER_SIZE) break
                    size = Mp4Box.readUInt64(buf, 8)
                    headerSize = Mp4Box.LARGE_HEADER_SIZE
                } else if (size == 0L) {
                    size = length - pos
                }
                if (size < headerSize || pos + size > length) break
                if (type == Mp4Box.TYPE_MDAT) {
                    var remaining = size - headerSize
                    raf.seek(pos + headerSize)
                    while (remaining > 0) {
                        val n = raf.read(buf, 0, minOf(buf.size.toLong(), remaining).toInt())
                        if (n <= 0) break
                        md.update(buf, 0, n)
                        remaining -= n
                    }
                }
                pos += size
            }
        }
        return md.digest().toHex()
    }

    private fun ByteArray.toHex(): String {
        val sb = StringBuilder(size * 2)
        for (b in this) {
            val v = b.toInt() and 0xFF
            sb.append(HEX_CHARS[v ushr 4]).append(HEX_CHARS[v and 0x0F])
        }
        return sb.toString()
    }

    private val HEX_CHARS = "0123456789abcdef".toCharArray()

    private fun replace(tmp: File, target: File): Boolean {
        if (tmp.renameTo(target)) return true
        val backup = File(target.parentFile ?: return false, target.name + ".mp4tag.bak")
        if (backup.exists()) backup.delete()
        if (!target.renameTo(backup)) return false
        if (tmp.renameTo(target)) {
            backup.delete()
            return true
        }
        backup.renameTo(target)
        return false
    }
}

/**
 * 只读解析器：读不到任何字段时返回空 [Mp4Tags]，不抛异常。
 */
object Mp4TagReader {

    private const val MAX_MOOV_BYTES = 64L * 1024 * 1024

    private const val TAG_TITLE = "\u00A9nam"
    private const val TAG_ARTIST = "\u00A9ART"
    private const val TAG_ALBUM = "\u00A9alb"
    private const val TAG_LYRICS = "\u00A9lyr"
    private const val TAG_COVER = "covr"
    private const val TAG_FREEFORM = "----"

    fun read(file: File): Mp4Tags = try {
        readInternal(file)
    } catch (t: Throwable) {
        Mp4Tags()
    }

    private fun readInternal(file: File): Mp4Tags {
        if (!file.isFile || file.length() < Mp4Box.HEADER_SIZE) return Mp4Tags()
        val top = Mp4Box.parse(file)
        val moov = top.firstOrNull { it.type == Mp4Box.TYPE_MOOV } ?: return Mp4Tags()
        if (moov.size > MAX_MOOV_BYTES || moov.size < moov.headerSize) return Mp4Tags()
        val moovBytes = readRange(file, moov.offset, moov.size.toInt()) ?: return Mp4Tags()
        val udta = Mp4Box.parse(moovBytes, moov.headerSize, moovBytes.size)
            .firstOrNull { it.type == Mp4Box.TYPE_UDTA } ?: return Mp4Tags()
        val udtaBytes = Mp4Box.slice(moovBytes, udta)
        val meta = Mp4Box.parse(udtaBytes, udta.headerSize, udtaBytes.size)
            .firstOrNull { it.type == Mp4Box.TYPE_META } ?: return Mp4Tags()
        val metaBytes = Mp4Box.slice(udtaBytes, meta)
        val ilst = Mp4Box.parse(metaBytes, meta.headerSize + Mp4Box.META_HEADER_SKIP, metaBytes.size)
            .firstOrNull { it.type == Mp4Box.TYPE_ILST } ?: return Mp4Tags()
        val ilstBytes = Mp4Box.slice(metaBytes, ilst)

        var title = ""
        var artist = ""
        var album = ""
        var lyrics = ""
        var cover: ByteArray? = null
        var delayMs = 0
        var narmeta = ""

        for (atom in Mp4Box.parse(ilstBytes, ilst.headerSize, ilstBytes.size)) {
            when (atom.type) {
                TAG_TITLE -> title = readDataContent(ilstBytes, atom)?.toString(Charsets.UTF_8) ?: ""
                TAG_ARTIST -> artist = readDataContent(ilstBytes, atom)?.toString(Charsets.UTF_8) ?: ""
                TAG_ALBUM -> album = readDataContent(ilstBytes, atom)?.toString(Charsets.UTF_8) ?: ""
                TAG_LYRICS -> lyrics = readDataContent(ilstBytes, atom)?.toString(Charsets.UTF_8) ?: ""
                TAG_COVER -> cover = readDataContent(ilstBytes, atom)
                TAG_FREEFORM -> {
                    val children = Mp4Box.parse(
                        ilstBytes, atom.offset.toInt() + atom.headerSize, atom.end.toInt()
                    )
                    val mean = children.firstOrNull { it.type == "mean" }
                        ?.let { readFullBoxText(ilstBytes, it) }
                    val name = children.firstOrNull { it.type == "name" }
                        ?.let { readFullBoxText(ilstBytes, it) }
                    if (mean == Mp4TagWriter.FREEFORM_MEAN) {
                        val value = readDataContent(ilstBytes, atom)?.toString(Charsets.UTF_8)
                        if (name == Mp4TagWriter.FREEFORM_DELAY) delayMs = value?.trim()?.toIntOrNull() ?: 0
                        if (name == Mp4TagWriter.FREEFORM_NARMETA) narmeta = value ?: ""
                    }
                }
            }
        }
        return Mp4Tags(title, artist, album, lyrics, cover, delayMs, narmeta)
    }

    private fun readRange(file: File, offset: Long, length: Int): ByteArray? {
        if (length < 0) return null
        val buf = ByteArray(length)
        RandomAccessFile(file, "r").use { raf ->
            raf.seek(offset)
            var read = 0
            while (read < length) {
                val n = raf.read(buf, read, length - read)
                if (n <= 0) return null
                read += n
            }
        }
        return buf
    }

    private fun readFullBoxText(container: ByteArray, box: Mp4BoxInfo): String {
        val start = box.offset.toInt() + box.headerSize + 4
        val end = box.end.toInt()
        if (start >= end) return ""
        return String(container, start, end - start, Charsets.UTF_8)
    }

    private fun readDataContent(container: ByteArray, atom: Mp4BoxInfo): ByteArray? {
        val children = Mp4Box.parse(container, atom.offset.toInt() + atom.headerSize, atom.end.toInt())
        val data = children.firstOrNull { it.type == "data" } ?: return null
        val start = data.offset.toInt() + data.headerSize + 8
        val end = data.end.toInt()
        if (start > end) return null
        return container.copyOfRange(start, end)
    }
}
