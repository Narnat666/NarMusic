package com.narnat.music

import com.narnat.music.core.mp4.Mp4Box
import com.narnat.music.core.mp4.Mp4BoxInfo
import com.narnat.music.core.mp4.Mp4TagReader
import com.narnat.music.core.mp4.Mp4TagWriter
import com.narnat.music.core.mp4.Mp4Tags
import java.io.File
import java.nio.file.Files
import java.security.MessageDigest
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * 覆盖 `moov` 在尾 / 不在尾两种布局，以及「变大平移」「变小 free 补齐」「无 moov 失败」三个分支。
 */
class Mp4TagWriterTest {

    // ---------- 布局 A：moov 是最后一个顶层 box（模拟 MediaMuxer 输出） ----------

    @Test
    fun writeWithMoovAtEndKeepsMdatAndStco() {
        val file = newTempFile("layout-a.m4a")
        val audio = ByteArray(4096) { (it % 251).toByte() }
        val ftyp = buildBox("ftyp", bytes("isom") + u32(0x200) + bytes("isomiso2mp41"))
        val mdatOffset = ftyp.size
        val mdatPayloadOffset = mdatOffset + 8
        val mdat = buildBox("mdat", audio)
        val moov = buildMoov(listOf(mdatPayloadOffset.toLong()))
        file.writeBytes(ftyp + mdat + moov)

        val cover = fakeImage(1024, png = true)
        val tags = Mp4Tags(
            title = "稻香",
            artist = "周杰伦",
            album = "魔杰座",
            lyrics = "[00:01.00]对这个世界如果你有太多的抱怨\n",
            cover = cover,
            delayMs = 250,
            narmeta = "https://www.bilibili.com/video/BV1aB4y1u7ox|酷狗音乐|+250|稻香"
        )
        assertTrue(Mp4TagWriter.write(file, tags))

        val read = Mp4TagReader.read(file)
        assertEquals(tags.title, read.title)
        assertEquals(tags.artist, read.artist)
        assertEquals(tags.album, read.album)
        assertEquals(tags.lyrics, read.lyrics)
        assertEquals(tags.narmeta, read.narmeta)
        assertEquals(tags.delayMs, read.delayMs)
        assertNotNull(read.cover)
        assertArrayEquals(cover, read.cover)

        val top = Mp4Box.parse(file)
        assertEquals(listOf("ftyp", "mdat", "moov"), top.map { it.type })

        val mdatBox = top.first { it.type == "mdat" }
        assertArrayEquals(audio, payloadOf(file, mdatBox))
        assertEquals(listOf(mdatPayloadOffset.toLong()), stcoOffsets(file))
    }

    // ---------- 布局 B：moov 不在末尾（模拟 B 站 fMP4），新 moov 变大 ----------

    @Test
    fun writeFragmentedMoovBeforeTailShiftsMoof() {
        val file = newTempFile("layout-b.m4a")
        val audio = ByteArray(2048) { (it * 7 % 253).toByte() }
        val ftyp = buildBox("ftyp", bytes("iso5") + u32(0x200) + bytes("iso5dash"))
        val moov = buildMoov(emptyList())
        val sidx = buildBox("sidx", ByteArray(24) { it.toByte() })
        val moof = buildBox("moof", buildBox("mfhd", ByteArray(8)) + buildTraf(1, null))
        val mdat = buildBox("mdat", audio)
        file.writeBytes(ftyp + moov + sidx + moof + mdat)

        val beforeMoofOffset = Mp4Box.parse(file).first { it.type == "moof" }.offset
        val beforeSidx = payloadOf(file, Mp4Box.parse(file).first { it.type == "sidx" })
        val origMoovSize = moov.size

        val cover = fakeImage(300 * 1024, png = false)
        val tags = Mp4Tags(
            title = "回家吧",
            artist = "光遇琴谱",
            album = "指弹",
            lyrics = "[00:00.00]回家吧\n",
            cover = cover,
            delayMs = -500,
            narmeta = "https://www.bilibili.com/video/BV1aB4y1u7ox|酷狗音乐|-500|回家吧"
        )
        assertTrue(Mp4TagWriter.write(file, tags))

        val read = Mp4TagReader.read(file)
        assertEquals(tags.title, read.title)
        assertEquals(tags.artist, read.artist)
        assertEquals(tags.album, read.album)
        assertEquals(tags.lyrics, read.lyrics)
        assertEquals(tags.narmeta, read.narmeta)
        assertEquals(tags.delayMs, read.delayMs)
        assertArrayEquals(cover, read.cover)

        val after = Mp4Box.parse(file)
        val newMoov = after.first { it.type == "moov" }
        val delta = newMoov.size - origMoovSize
        assertTrue("新 moov 必须变大才能触发平移分支", delta > 0)

        val afterMoofOffset = after.first { it.type == "moof" }.offset
        assertEquals(beforeMoofOffset + delta, afterMoofOffset)

        val afterSidx = payloadOf(file, after.first { it.type == "sidx" })
        assertArrayEquals("sidx 全是相对锚点，必须原样复制", beforeSidx, afterSidx)

        val mdatBox = after.first { it.type == "mdat" }
        assertArrayEquals(audio, payloadOf(file, mdatBox))
    }

    // ---------- 布局 B + 非空 stco：变大时 stco 必须整体平移 ----------

    @Test
    fun writeShiftsStcoWhenMoovGrows() {
        val file = newTempFile("layout-b-stco.m4a")
        val audio = ByteArray(1024) { it.toByte() }
        val ftyp = buildBox("ftyp", bytes("iso5") + u32(0x200) + bytes("iso5dash"))
        val originalChunkOffset = 4096L
        val moov = buildMoov(listOf(originalChunkOffset))
        val moof = buildBox("moof", buildBox("mfhd", ByteArray(8)) + buildTraf(1, null))
        val mdat = buildBox("mdat", audio)
        file.writeBytes(ftyp + moov + moof + mdat)

        val origMoovSize = Mp4Box.parse(file).first { it.type == "moov" }.size
        val cover = fakeImage(64 * 1024, png = true)
        val tags = Mp4Tags(title = "Lemon", artist = "米津玄師", cover = cover)
        assertTrue(Mp4TagWriter.write(file, tags))

        val newMoovSize = Mp4Box.parse(file).first { it.type == "moov" }.size
        val delta = newMoovSize - origMoovSize
        assertTrue(delta > 0)
        assertEquals(listOf(originalChunkOffset + delta), stcoOffsets(file))
        assertArrayEquals(audio, payloadOf(file, Mp4Box.parse(file).first { it.type == "mdat" }))
    }

    // ---------- 布局 B：新 moov 变小，必须用 free 补齐且后段字节位置不变 ----------

    @Test
    fun writePadsWithFreeWhenMoovShrinks() {
        val file = newTempFile("layout-b-shrink.m4a")
        val audio = ByteArray(1536) { (it % 97).toByte() }
        val ftyp = buildBox("ftyp", bytes("iso5") + u32(0x200) + bytes("iso5dash"))
        val bigCover = fakeImage(200 * 1024, png = true)
        val moovWithTag = buildMoovWithIlst(emptyList(), bigCover)
        val sidx = buildBox("sidx", ByteArray(16) { 0x11 })
        val moof = buildBox("moof", buildBox("mfhd", ByteArray(8)) + buildTraf(1, null))
        val mdat = buildBox("mdat", audio)
        file.writeBytes(ftyp + moovWithTag + sidx + moof + mdat)

        val beforeTop = Mp4Box.parse(file)
        val beforeMoofOffset = beforeTop.first { it.type == "moof" }.offset
        val beforeMdatOffset = beforeTop.first { it.type == "mdat" }.offset

        val smallCover = fakeImage(4096, png = true)
        val tags = Mp4Tags(title = "小封面", cover = smallCover)
        assertTrue(Mp4TagWriter.write(file, tags))

        val after = Mp4Box.parse(file)
        assertTrue("变小分支应插入 free box", after.any { it.type == "free" })
        assertEquals(beforeMoofOffset, after.first { it.type == "moof" }.offset)
        assertEquals(beforeMdatOffset, after.first { it.type == "mdat" }.offset)
        assertArrayEquals(audio, payloadOf(file, after.first { it.type == "mdat" }))
        assertArrayEquals(smallCover, Mp4TagReader.read(file).cover)
        assertEquals("小封面", Mp4TagReader.read(file).title)
    }

    // ---------- 布局 B：变大时 tfhd.base_data_offset 也必须平移 ----------

    @Test
    fun writePatchesTfhdBaseDataOffset() {
        val file = newTempFile("layout-b-tfhd.m4a")
        val audio = ByteArray(1024) { it.toByte() }
        val ftyp = buildBox("ftyp", bytes("iso5") + u32(0x200) + bytes("iso5dash"))
        val moov = buildMoov(emptyList())
        val placeholder = buildBox("moof", buildBox("mfhd", ByteArray(8)) + buildTraf(1, 0L))
        val mdatPayloadOffset = ftyp.size + moov.size + placeholder.size + 8
        val moof = buildBox(
            "moof",
            buildBox("mfhd", ByteArray(8)) + buildTraf(1, mdatPayloadOffset.toLong())
        )
        val mdat = buildBox("mdat", audio)
        file.writeBytes(ftyp + moov + moof + mdat)

        val origMoovSize = Mp4Box.parse(file).first { it.type == "moov" }.size
        assertEquals(mdatPayloadOffset.toLong(), tfhdBaseDataOffset(file))

        val cover = fakeImage(48 * 1024, png = false)
        assertTrue(Mp4TagWriter.write(file, Mp4Tags(title = "tfhd", cover = cover)))

        val newMoovSize = Mp4Box.parse(file).first { it.type == "moov" }.size
        val delta = newMoovSize - origMoovSize
        assertTrue(delta > 0)
        assertEquals(mdatPayloadOffset + delta, tfhdBaseDataOffset(file))
        assertArrayEquals(audio, payloadOf(file, Mp4Box.parse(file).first { it.type == "mdat" }))
    }

    // ---------- 失败分支：没有 moov 必须返回 false 且不动原文件 ----------

    @Test
    fun writeFailsWhenMoovMissing() {
        val file = newTempFile("no-moov.m4a")
        val bytes = buildBox("ftyp", bytes("isom") + u32(0x200) + bytes("isom")) +
            buildBox("mdat", ByteArray(512) { it.toByte() })
        file.writeBytes(bytes)
        val before = md5(file)

        val tags = Mp4Tags(title = "没有 moov", artist = "nobody", cover = fakeImage(6000, png = false))
        assertEquals(false, Mp4TagWriter.write(file, tags))
        assertEquals(before, md5(file))
        assertEquals(Mp4Tags(), Mp4TagReader.read(file))
    }

    // ---------- 辅助 ----------

    private fun newTempFile(name: String): File {
        val dir = Files.createTempDirectory("narmp4").toFile()
        dir.deleteOnExit()
        val file = File(dir, name)
        file.deleteOnExit()
        return file
    }

    private fun bytes(text: String): ByteArray = text.toByteArray(Charsets.ISO_8859_1)

    private fun u32(value: Int): ByteArray {
        val out = ByteArray(4)
        writeU32(out, 0, value.toLong())
        return out
    }

    private fun writeU32(target: ByteArray, offset: Int, value: Long) {
        target[offset] = ((value ushr 24) and 0xFF).toByte()
        target[offset + 1] = ((value ushr 16) and 0xFF).toByte()
        target[offset + 2] = ((value ushr 8) and 0xFF).toByte()
        target[offset + 3] = (value and 0xFF).toByte()
    }

    private fun buildBox(type: String, payload: ByteArray): ByteArray {
        val out = ByteArray(payload.size + 8)
        writeU32(out, 0, (payload.size + 8).toLong())
        bytes(type).copyInto(out, 4)
        payload.copyInto(out, 8)
        return out
    }

    private fun buildStco(offsets: List<Long>): ByteArray {
        val payload = ByteArray(8 + offsets.size * 4)
        writeU32(payload, 0, 0L)
        writeU32(payload, 4, offsets.size.toLong())
        offsets.forEachIndexed { index, value -> writeU32(payload, 8 + index * 4, value) }
        return buildBox("stco", payload)
    }

    private fun buildMoov(chunkOffsets: List<Long>): ByteArray {
        val stbl = buildBox("stbl", buildStco(chunkOffsets))
        val minf = buildBox("minf", stbl)
        val mdia = buildBox("mdia", minf)
        val trak = buildBox("trak", mdia)
        val mvhd = buildBox("mvhd", ByteArray(100))
        return buildBox("moov", mvhd + trak)
    }

    private fun buildMoovWithIlst(chunkOffsets: List<Long>, cover: ByteArray): ByteArray {
        val stbl = buildBox("stbl", buildStco(chunkOffsets))
        val minf = buildBox("minf", stbl)
        val mdia = buildBox("mdia", minf)
        val trak = buildBox("trak", mdia)
        val mvhd = buildBox("mvhd", ByteArray(100))
        val covrPayload = ByteArray(8 + cover.size)
        writeU32(covrPayload, 0, 0x0EL)
        cover.copyInto(covrPayload, 8)
        val ilst = buildBox("ilst", buildBox("covr", buildBox("data", covrPayload)))
        val hdlr = buildBox("hdlr", ByteArray(25).also { bytes("mdir").copyInto(it, 8) })
        val meta = buildBox("meta", ByteArray(4) + hdlr + ilst)
        val udta = buildBox("udta", meta)
        return buildBox("moov", mvhd + trak + udta)
    }

    private fun buildTraf(trackId: Int, baseDataOffset: Long?): ByteArray {
        val flags = if (baseDataOffset != null) 0x01L else 0x00L
        val payload = ByteArray(8 + if (baseDataOffset != null) 8 else 0)
        writeU32(payload, 0, flags)
        writeU32(payload, 4, trackId.toLong())
        if (baseDataOffset != null) {
            writeU32(payload, 8, baseDataOffset ushr 32)
            writeU32(payload, 12, baseDataOffset and 0xFFFFFFFFL)
        }
        val tfhd = buildBox("tfhd", payload)
        val tfdt = buildBox("tfdt", ByteArray(8))
        val trun = buildBox("trun", ByteArray(12))
        return buildBox("traf", tfhd + tfdt + trun)
    }

    private fun fakeImage(size: Int, png: Boolean): ByteArray {
        val bytes = ByteArray(size) { (it * 31 % 253).toByte() }
        if (png) {
            bytes[0] = 0x89.toByte()
            bytes[1] = 0x50
            bytes[2] = 0x4E
            bytes[3] = 0x47
        } else {
            bytes[0] = 0xFF.toByte()
            bytes[1] = 0xD8.toByte()
            bytes[2] = 0xFF.toByte()
            bytes[3] = 0xE0.toByte()
        }
        return bytes
    }

    private fun payloadOf(file: File, box: Mp4BoxInfo): ByteArray {
        val all = file.readBytes()
        return all.copyOfRange(box.offset.toInt() + box.headerSize, box.end.toInt())
    }

    private fun tfhdBaseDataOffset(file: File): Long? {
        val all = file.readBytes()
        val tfhd = findBoxes(all, 0, all.size, "tfhd").firstOrNull() ?: return null
        val base = tfhd.offset.toInt() + tfhd.headerSize
        val flags = Mp4Box.readUInt32(all, base) and 0xFFFFFFL
        if ((flags and 0x01L) == 0L) return null
        return Mp4Box.readUInt64(all, base + 8)
    }

    private fun stcoOffsets(file: File): List<Long> {
        val all = file.readBytes()
        val stco = findBoxes(all, 0, all.size, "stco").firstOrNull()
            ?: return emptyList()
        val base = stco.offset.toInt() + stco.headerSize
        val count = Mp4Box.readUInt32(all, base + 4).toInt()
        return (0 until count).map { Mp4Box.readUInt32(all, base + 8 + it * 4) }
    }

    private fun findBoxes(
        bytes: ByteArray,
        start: Int,
        end: Int,
        type: String,
        acc: MutableList<Mp4BoxInfo> = ArrayList()
    ): List<Mp4BoxInfo> {
        for (box in Mp4Box.parse(bytes, start, end)) {
            if (box.type == type) {
                acc.add(box)
            } else if (box.type in Mp4Box.CONTAINER_TYPES) {
                findBoxes(bytes, box.offset.toInt() + box.headerSize, box.end.toInt(), type, acc)
            }
        }
        return acc
    }

    private fun md5(file: File): String {
        val digest = MessageDigest.getInstance("MD5").digest(file.readBytes())
        return digest.joinToString("") { "%02x".format(it) }
    }
}
