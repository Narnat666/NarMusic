package com.narnat.music.data.library

import android.content.ContentResolver
import android.content.ContentValues
import android.content.Context
import android.content.Intent
import android.content.IntentSender
import android.net.Uri
import androidx.core.net.toUri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import androidx.annotation.RequiresApi
import com.narnat.music.data.db.MusicFileDao
import com.narnat.music.data.db.MusicFileEntity
import java.io.File
import java.io.IOException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * 文件落地 + 库表 + 系统交互。文件写到公共音乐目录 `Music/NarMusic/`。
 */
class MusicLibraryRepository(
    private val context: Context,
    private val dao: MusicFileDao
) {

    data class PublishedFile(
        val mediaUri: String,
        val absolutePath: String,
        val sizeBytes: Long
    )

    sealed class DeleteOutcome {
        object Deleted : DeleteOutcome()

        /** 需要 UI 用 `startIntentSenderForResult` 拉起系统授权。 */
        class NeedsUserConfirm(val intent: Intent) : DeleteOutcome()

        class Failed(val reason: String) : DeleteOutcome()
    }

    private val resolver: ContentResolver get() = context.contentResolver

    suspend fun publishM4a(tempFile: File, displayName: String): PublishedFile =
        withContext(Dispatchers.IO) {
            if (!tempFile.isFile) throw IOException("待发布文件不存在: ${tempFile.name}")
            val fileName = uniqueFileName(sanitizeName(displayName))
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                try {
                    return@withContext publishToMediaStore(tempFile, fileName)
                } catch (e: Exception) {
                    // 回退到应用私有音乐目录（无需权限）
                }
            }
            publishToAppMusicDir(tempFile, fileName)
        }

    @RequiresApi(Build.VERSION_CODES.Q)
    private fun publishToMediaStore(tempFile: File, fileName: String): PublishedFile {
        val values = ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, fileName)
            put(MediaStore.MediaColumns.MIME_TYPE, MIME_TYPE)
            put(MediaStore.MediaColumns.RELATIVE_PATH, RELATIVE_FOLDER)
            put(MediaStore.MediaColumns.IS_PENDING, 1)
        }
        val collection = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI
        val uri = resolver.insert(collection, values) ?: throw IOException("MediaStore insert 失败")
        try {
            val out = resolver.openOutputStream(uri) ?: throw IOException("openOutputStream 返回 null")
            out.use { output -> tempFile.inputStream().use { it.copyTo(output) } }
            val publish = ContentValues().apply {
                put(MediaStore.MediaColumns.IS_PENDING, 0)
            }
            resolver.update(uri, publish, null, null)
        } catch (t: Exception) {
            resolver.delete(uri, null, null)
            throw t
        }
        val size = tempFile.length()
        val absolutePath = queryDataPath(uri) ?: defaultAbsolutePath(fileName)
        return PublishedFile(uri.toString(), absolutePath, size)
    }

    private fun publishToAppMusicDir(tempFile: File, fileName: String): PublishedFile {
        val dir = context.getExternalFilesDir(Environment.DIRECTORY_MUSIC) ?: context.filesDir
        dir.mkdirs()
        val target = File(dir, fileName)
        tempFile.inputStream().use { input ->
            target.outputStream().use { output -> input.copyTo(output) }
        }
        return PublishedFile(Uri.fromFile(target).toString(), target.absolutePath, target.length())
    }

    /**
     * 删除库记录；`countByFilePath <= 1` 时才真正删物理文件（同 C++ `remove` 的引用计数语义）。
     */
    suspend fun deleteFile(item: MusicFileEntity): DeleteOutcome = withContext(Dispatchers.IO) {
        if (dao.countByFilePath(item.filePath) <= 1) {
            val outcome = deletePhysical(item)
            if (outcome !is DeleteOutcome.Deleted) return@withContext outcome
        }
        dao.deleteById(item.id)
        DeleteOutcome.Deleted
    }

    /** 只删磁盘上的旧文件、保留库记录（重复下载替换文件时用）。 */
    suspend fun removeStoredFile(item: MusicFileEntity): Boolean = withContext(Dispatchers.IO) {
        deletePhysical(item) is DeleteOutcome.Deleted
    }

    private fun deletePhysical(item: MusicFileEntity): DeleteOutcome {
        val mediaUri = item.mediaUri
        if (mediaUri.startsWith("content://")) {
            val uri = mediaUri.toUri()
            try {
                val rows = resolver.delete(uri, null, null)
                if (rows > 0) return DeleteOutcome.Deleted
            } catch (e: SecurityException) {
                return permissionOutcome(uri, e)
            } catch (e: IllegalArgumentException) {
                return DeleteOutcome.Failed("无法删除 MediaStore 记录: ${e.message}")
            }
        }
        val file = File(item.filePath)
        if (!file.exists()) return DeleteOutcome.Deleted
        return if (file.delete()) {
            DeleteOutcome.Deleted
        } else {
            DeleteOutcome.Failed("文件删除失败: ${item.filePath}")
        }
    }

    private fun permissionOutcome(uri: Uri, error: SecurityException): DeleteOutcome {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return try {
                val pending = MediaStore.createDeleteRequest(resolver, listOf(uri))
                DeleteOutcome.NeedsUserConfirm(intentForSender(pending.intentSender))
            } catch (t: Exception) {
                DeleteOutcome.Failed("创建系统删除请求失败: ${t.message}")
            }
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && error is android.app.RecoverableSecurityException) {
            return try {
                DeleteOutcome.NeedsUserConfirm(
                    intentForSender(error.userAction.actionIntent.intentSender)
                )
            } catch (t: Exception) {
                DeleteOutcome.Failed("获取授权 Intent 失败: ${t.message}")
            }
        }
        return DeleteOutcome.Failed("没有删除该文件的权限")
    }

    private fun intentForSender(sender: IntentSender): Intent =
        Intent().putExtra(EXTRA_INTENT_SENDER, sender)

    /** 优先用系统文件管理器打开 `Music/NarMusic`；不可用时返回 null，由 UI 提示已复制路径。 */
    fun openFolderIntent(item: MusicFileEntity): Intent? {
        val direct = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(FOLDER_URI.toUri(), "resource/folder")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        if (direct.resolveActivity(context.packageManager) != null) return direct
        val root = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(ROOT_URI.toUri(), "resource/folder")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        if (root.resolveActivity(context.packageManager) != null) return root
        return null
    }

    fun absolutePathOf(item: MusicFileEntity): String = item.filePath

    /** 核对每条记录的文件是否仍存在；不存在的把 `file_size` 归零（[MusicFileEntity.isMissing]）。 */
    suspend fun refresh(): Int = withContext(Dispatchers.IO) {
        var missing = 0
        for (item in dao.findAll()) {
            if (exists(item)) {
                if (item.fileSize <= 0L) {
                    val size = File(item.filePath).length()
                    if (size > 0L) dao.update(item.copy(fileSize = size))
                }
            } else {
                missing++
                if (item.fileSize != 0L) dao.update(item.copy(fileSize = 0L))
            }
        }
        missing
    }

    private fun exists(item: MusicFileEntity): Boolean {
        if (item.mediaUri.startsWith("content://")) {
            try {
                resolver.openFileDescriptor(item.mediaUri.toUri(), "r")?.use { return true }
            } catch (e: Exception) {
                // 打开失败则退回文件系统检查
            }
        }
        val file = File(item.filePath)
        return file.isFile && file.length() > 0L
    }

    private fun queryDataPath(uri: Uri): String? {
        val projection = arrayOf(MediaStore.MediaColumns.DATA)
        return try {
            resolver.query(uri, projection, null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) {
                    val index = cursor.getColumnIndex(MediaStore.MediaColumns.DATA)
                    if (index >= 0) cursor.getString(index) else null
                } else {
                    null
                }
            }
        } catch (e: Exception) {
            null
        }
    }

    private fun defaultAbsolutePath(fileName: String): String =
        "${Environment.getExternalStorageDirectory().absolutePath}/$RELATIVE_FOLDER/$fileName"

    private fun sanitizeName(displayName: String): String {
        val trimmed = displayName.trim()
        val base = if (trimmed.isEmpty()) "music_${System.currentTimeMillis()}" else trimmed
        val sb = StringBuilder(base.length)
        for (c in base) {
            sb.append(if (c in ILLEGAL_CHARS) '_' else c)
        }
        return sb.toString().trim().ifEmpty { "music_${System.currentTimeMillis()}" }
    }

    private fun uniqueFileName(nameWithoutExt: String): String {
        val first = "$nameWithoutExt.m4a"
        if (!nameTaken(first)) return first
        val stamp = System.currentTimeMillis()
        var candidate = "${nameWithoutExt}_$stamp.m4a"
        var suffix = 1
        while (nameTaken(candidate)) {
            candidate = "${nameWithoutExt}_${stamp}_$suffix.m4a"
            suffix++
        }
        return candidate
    }

    private fun nameTaken(fileName: String): Boolean {
        val guess = File(defaultAbsolutePath(fileName))
        if (guess.exists()) return true
        val projection = arrayOf(MediaStore.MediaColumns._ID)
        val selection = "${MediaStore.MediaColumns.DISPLAY_NAME} = ?"
        return try {
            resolver.query(
                MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
                projection,
                selection,
                arrayOf(fileName),
                null
            )?.use { it.count > 0 } ?: false
        } catch (e: Exception) {
            false
        }
    }

    companion object {
        const val RELATIVE_FOLDER = "Music/NarMusic"
        const val MIME_TYPE = "audio/mp4"
        const val EXTRA_INTENT_SENDER = "com.narnat.music.EXTRA_INTENT_SENDER"

        private const val FOLDER_URI =
            "content://com.android.externalstorage.documents/document/primary:Music%2FNarMusic"
        private const val ROOT_URI = "content://com.android.externalstorage.documents/root/primary"
        private val ILLEGAL_CHARS = charArrayOf('\\', '/', ':', '*', '?', '"', '<', '>', '|')
    }
}
