package com.narnat.music

import com.narnat.music.core.lyrics.KrcDecoder
import java.io.ByteArrayOutputStream
import java.util.Base64
import java.util.zip.Deflater
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class KrcDecoderTest {

    @Test
    fun decryptsKrcAndPicksChineseTranslation() {
        val english = """{"language":0,"lyricContent":[["english line one"],["english line two"]]}"""
        val chinese = """{"language":0,"lyricContent":[["第一行"],["第二行"]]}"""
        val langJson = """{"content":[$english,$chinese]}"""
        val langBase64 = Base64.getEncoder().encodeToString(langJson.toByteArray(Charsets.UTF_8))
        val fullText = "[ti:稻香]\n" +
            "[0,100]<0,50,0>稻<50,50,0>香\n" +
            "[1000,200]<0,100,0>第二句\n" +
            "[language:$langBase64]"

        val krc = buildKrc(fullText)
        val decoded = KrcDecoder.decryptKrc(krc)
        assertTrue(decoded != null)
        val lyrics = decoded!!.first
        val translation = decoded.second
        assertTrue(lyrics.contains("[ti:稻香]"))
        assertTrue(lyrics.contains("[0,100]"))
        assertTrue("正文不应包含 language 段", !lyrics.contains("[language:"))
        assertEquals("[00:00.00]第一行\n[00:01.00]第二行\n", translation)
    }

    @Test
    fun decryptsKrcWithoutLanguageSegment() {
        val decoded = KrcDecoder.decryptKrc(buildKrc("[0,100]<0,50,0>alone"))
        assertTrue(decoded != null)
        assertEquals("", decoded!!.second)
        assertTrue(decoded.first.contains("alone"))
    }

    @Test
    fun rejectsWrongMagic() {
        val data = buildKrc("[0,100]<0,50,0>x")
        data[0] = 'x'.code.toByte()
        assertNull(KrcDecoder.decryptKrc(data))
        assertNull(KrcDecoder.decryptKrc(ByteArray(2)))
    }

    @Test
    fun convertsKrcStringToLrc() {
        val lrc = KrcDecoder.krcStringToLrc(
            "[0,100]<0,50,0>Hello<50,50,0> World\n[1000,200]<0,100,0>Second"
        )
        assertEquals("[00:00.000]Hello World\n[00:01.000]Second\n", lrc)
    }

    @Test
    fun convertsKrcJsonToLrc() {
        val json = """
            {"sentences":[
              {"startMs":0,"words":[{"text":"抬头看见那"}],"translation":"Look up"},
              {"startMs":3670,"words":[{"startMs":3670,"text":"微笑的雪花"}]},
              {"words":[{"startMs":8910,"text":"你别出现"}]}
            ]}
        """.trimIndent()
        val (lrc, trans) = KrcDecoder.krcJsonToLrc(json)
        assertEquals(
            "[00:00.000]抬头看见那\n[00:03.670]微笑的雪花\n[00:08.910]你别出现\n",
            lrc
        )
        assertEquals("[00:00.000]Look up\n", trans)
    }

    private fun buildKrc(text: String): ByteArray {
        val raw = text.toByteArray(Charsets.UTF_8)
        val deflater = Deflater()
        deflater.setInput(raw)
        deflater.finish()
        val compressed = ByteArrayOutputStream()
        val buffer = ByteArray(4096)
        while (!deflater.finished()) {
            val n = deflater.deflate(buffer)
            compressed.write(buffer, 0, n)
        }
        deflater.end()
        val payload = compressed.toByteArray()
        val result = ByteArray(4 + payload.size)
        result[0] = 'k'.code.toByte()
        result[1] = 'r'.code.toByte()
        result[2] = 'c'.code.toByte()
        result[3] = '1'.code.toByte()
        for (i in payload.indices) {
            result[4 + i] = (payload[i].toInt() xor XOR_KEY[i % 16].toInt()).toByte()
        }
        return result
    }

    companion object {
        private val XOR_KEY = byteArrayOf(
            64, 71, 97, 119, 94, 50, 116, 71, 81, 54, 49, 45, 206.toByte(), 210.toByte(), 110, 105
        )
    }
}
