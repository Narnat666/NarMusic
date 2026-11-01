package com.narnat.music

import android.content.Context
import androidx.room.Room
import androidx.test.core.app.ApplicationProvider
import com.narnat.music.data.db.MusicDatabase
import com.narnat.music.data.db.MusicFileDao
import com.narnat.music.data.db.MusicFileEntity
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [34])
class MusicFileDaoTest {

    private lateinit var db: MusicDatabase
    private lateinit var dao: MusicFileDao

    @Before
    fun setUp() {
        val context = ApplicationProvider.getApplicationContext<Context>()
        db = Room.inMemoryDatabaseBuilder(context, MusicDatabase::class.java)
            .allowMainThreadQueries()
            .build()
        dao = db.musicFileDao()
    }

    @After
    fun tearDown() {
        db.close()
    }

    @Test
    fun insertsAndFindsById() = runBlocking {
        val id = dao.insert(sample(songName = "稻香", downloadedAt = 100L))
        assertTrue(id > 0)
        val found = dao.findById(id)
        assertNotNull(found)
        assertEquals("稻香", found!!.songName)
        assertEquals("audio/mp4", found.mimeType)
        assertEquals("Music/NarMusic", found.relativePath)
        assertEquals(100L, found.downloadedAt)
    }

    @Test
    fun findsBySystemFilename() = runBlocking {
        dao.insert(sample(songName = "稻香", systemFilename = "稻香.m4a"))
        dao.insert(sample(songName = "晴天", systemFilename = "晴天.m4a"))
        assertEquals("晴天", dao.findBySystemFilename("晴天.m4a")?.songName)
        assertNull(dao.findBySystemFilename("不存在.m4a"))
    }

    @Test
    fun dedupQueryPrefersLatest() = runBlocking {
        dao.insert(sample(songName = "稻香", downloadedAt = 100L, filePath = "/a/1.m4a"))
        dao.insert(sample(songName = "稻香", downloadedAt = 500L, filePath = "/a/2.m4a"))
        val hit = dao.findBySongArtistPlatformDelay("稻香", "周杰伦", "酷狗音乐", 0)
        assertNotNull(hit)
        assertEquals("/a/2.m4a", hit!!.filePath)
        assertNull(dao.findBySongArtistPlatformDelay("稻香", "周杰伦", "酷狗音乐", 999))
    }

    @Test
    fun markUsedAndFindExpired() = runBlocking {
        val oldId = dao.insert(sample(songName = "旧歌", lastUsedAt = 100L))
        val freshId = dao.insert(sample(songName = "新歌", lastUsedAt = 1_000_000L))

        val expired = dao.findExpired(500_000L)
        assertEquals(1, expired.size)
        assertEquals(oldId, expired[0].id)

        dao.markUsed(freshId, 2_000_000L)
        val used = dao.findById(freshId)
        assertNotNull(used)
        assertTrue(used!!.inUse)
        assertEquals(2_000_000L, used.lastUsedAt)

        // in_use = 1 的记录不会被视为过期
        assertEquals(1, dao.findExpired(3_000_000L).size)
    }

    @Test
    fun deletesById() = runBlocking {
        val id = dao.insert(sample(songName = "稻香"))
        assertEquals(1, dao.countByFilePath("/music/周杰伦 - 稻香.m4a"))
        dao.deleteById(id)
        assertNull(dao.findById(id))
        assertEquals(0, dao.countByFilePath("/music/周杰伦 - 稻香.m4a"))
    }

    @Test
    fun observeAllOrdersByDownloadedAtDesc() = runBlocking {
        dao.insert(sample(songName = "早", downloadedAt = 10L))
        dao.insert(sample(songName = "晚", downloadedAt = 90L))
        dao.insert(sample(songName = "中", downloadedAt = 50L))
        val list = dao.observeAll().first()
        assertEquals(listOf("晚", "中", "早"), list.map { it.songName })
    }

    private fun sample(
        songName: String,
        artist: String = "周杰伦",
        platform: String = "酷狗音乐",
        delayMs: Int = 0,
        downloadedAt: Long = 1000L,
        lastUsedAt: Long = 1000L,
        systemFilename: String = "周杰伦 - 稻香.m4a",
        filePath: String = "/music/周杰伦 - 稻香.m4a"
    ): MusicFileEntity = MusicFileEntity(
        songName = songName,
        artist = artist,
        album = "魔杰座",
        filePath = filePath,
        systemFilename = systemFilename,
        originalFilename = songName,
        fileSize = 4186124L,
        delayMs = delayMs,
        platform = platform,
        inUse = false,
        downloadedAt = downloadedAt,
        lastUsedAt = lastUsedAt,
        lyrics = "[00:01.00]歌词",
        mediaUri = "content://media/external/audio/media/1",
        relativePath = "Music/NarMusic",
        mimeType = "audio/mp4",
        durationMs = 175253L
    )
}
