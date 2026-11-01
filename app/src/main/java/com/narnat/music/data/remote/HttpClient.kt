package com.narnat.music.data.remote

import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.concurrent.TimeUnit
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody

/**
 * OkHttp 封装：统一 UA、重试一次、下载带进度回调。
 */
class HttpClient(private val baseClient: OkHttpClient = defaultClient()) {

    class Resp(
        val success: Boolean,
        val statusCode: Int,
        val body: String,
        val bytes: ByteArray? = null
    ) {
        fun isEmpty(): Boolean = body.isEmpty()
    }

    fun get(url: String, headers: Map<String, String> = emptyMap()): Resp =
        execute(METHOD_GET, url, null, headers)

    fun postJson(
        url: String,
        bodyJson: String,
        headers: Map<String, String> = emptyMap()
    ): Resp = execute(METHOD_POST, url, bodyJson, headers)

    /**
     * 下载到 [destFile]。失败会删除残留文件。
     */
    fun download(
        url: String,
        destFile: File,
        headers: Map<String, String> = emptyMap(),
        onProgress: (Long) -> Unit = {}
    ): Boolean {
        var attempt = 0
        while (true) {
            var written = 0L
            try {
                val client = baseClient.newBuilder()
                    .readTimeout(DOWNLOAD_TIMEOUT_SEC, TimeUnit.SECONDS)
                    .callTimeout(DOWNLOAD_TIMEOUT_SEC * 2, TimeUnit.SECONDS)
                    .build()
                client.newCall(buildRequest(METHOD_GET, url, null, headers)).execute().use { response ->
                    if (!response.isSuccessful) {
                        destFile.delete()
                        return false
                    }
                    val body = response.body ?: run {
                        destFile.delete()
                        return false
                    }
                    destFile.parentFile?.mkdirs()
                    body.byteStream().use { input ->
                        FileOutputStream(destFile).use { out ->
                            val buffer = ByteArray(DOWNLOAD_BUFFER_SIZE)
                            while (true) {
                                val n = input.read(buffer)
                                if (n <= 0) break
                                out.write(buffer, 0, n)
                                written += n
                                onProgress(written)
                            }
                        }
                    }
                }
                if (written > 0L) return true
                destFile.delete()
                return false
            } catch (e: IOException) {
                destFile.delete()
                if (attempt >= MAX_RETRY) return false
                attempt++
            }
        }
    }

    /**
     * 跟随重定向并返回最终 URL（短链 `b23.tv/xxx` → `bilibili.com/video/BVxxx`）。
     * 必须禁用自动重定向，否则拿不到 `Location` 头。
     */
    fun resolveRedirect(url: String): String? {
        val client = baseClient.newBuilder()
            .followRedirects(false)
            .followSslRedirects(false)
            .build()
        var current = url
        repeat(MAX_REDIRECT) {
            try {
                client.newCall(buildRequest(METHOD_GET, current, null, emptyMap())).execute().use { response ->
                    val location = response.header("Location")
                    if (location.isNullOrEmpty()) {
                        return if (response.isSuccessful) current else null
                    }
                    current = when {
                        location.startsWith("http") -> location
                        else -> response.request.url.resolve(location)?.toString() ?: return null
                    }
                }
            } catch (e: IOException) {
                return null
            }
        }
        return null
    }

    private fun execute(
        method: String,
        url: String,
        bodyJson: String?,
        headers: Map<String, String>
    ): Resp {
        var attempt = 0
        while (true) {
            try {
                baseClient.newCall(buildRequest(method, url, bodyJson, headers)).execute().use { response ->
                    val bytes = response.body?.bytes()
                    val text = if (bytes == null) "" else String(bytes, Charsets.UTF_8)
                    return Resp(response.isSuccessful, response.code, text, bytes)
                }
            } catch (e: IOException) {
                if (attempt >= MAX_RETRY) return Resp(false, 0, "", null)
                attempt++
            }
        }
    }

    private fun buildRequest(
        method: String,
        url: String,
        bodyJson: String?,
        headers: Map<String, String>
    ): Request {
        val builder = Request.Builder().url(url)
        if (headers.keys.none { it.equals(HEADER_UA, ignoreCase = true) }) {
            builder.header(HEADER_UA, UA)
        }
        for ((k, v) in headers) builder.header(k, v)
        if (method == METHOD_POST) {
            builder.post((bodyJson ?: "").toRequestBody(JSON_MEDIA_TYPE))
        }
        return builder.build()
    }

    companion object {
        const val UA =
            "Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 (KHTML, like Gecko) " +
                "Chrome/124.0 Mobile Safari/537.36"

        const val HEADER_UA = "User-Agent"
        const val CONNECT_TIMEOUT_SEC = 10L
        const val READ_TIMEOUT_SEC = 30L
        const val DOWNLOAD_TIMEOUT_SEC = 120L
        private const val MAX_RETRY = 1
        private const val MAX_REDIRECT = 5
        private const val DOWNLOAD_BUFFER_SIZE = 8192
        private const val METHOD_GET = "GET"
        private const val METHOD_POST = "POST"
        private val JSON_MEDIA_TYPE = "application/json; charset=utf-8".toMediaType()

        fun defaultClient(): OkHttpClient = OkHttpClient.Builder()
            .connectTimeout(CONNECT_TIMEOUT_SEC, TimeUnit.SECONDS)
            .readTimeout(READ_TIMEOUT_SEC, TimeUnit.SECONDS)
            .writeTimeout(READ_TIMEOUT_SEC, TimeUnit.SECONDS)
            .build()
    }
}
