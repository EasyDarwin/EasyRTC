package cn.easydarwin.easyrtc.repository

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.MultipartBody
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.util.concurrent.TimeUnit
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import org.json.JSONException

data class LogUploadResult(
    val success: Boolean,
    val message: String
)

class LogUploadRepository(
    private val baseUrl: String = "http://47.94.13.1:9999"
) {
    private val client = OkHttpClient.Builder()
        .connectTimeout(15, TimeUnit.SECONDS)
        .readTimeout(15, TimeUnit.SECONDS)
        .writeTimeout(15, TimeUnit.SECONDS)
        .build()

    suspend fun uploadLogs(externalFilesDir: File?): LogUploadResult = withContext(Dispatchers.IO) {
        try {
            if (externalFilesDir == null) {
                return@withContext LogUploadResult(
                    success = false,
                    message = "外部日志目录不可用"
                )
            }

            val txtFiles = collectTextFiles(externalFilesDir)
            if (txtFiles.isEmpty()) {
                return@withContext LogUploadResult(
                    success = false,
                    message = "暂无可上传的 txt 文件"
                )
            }

            val zipBytes = createZipBytes(externalFilesDir, txtFiles)
            val requestBody = zipBytes.toRequestBody("application/zip".toMediaType())
            val multipartBody = MultipartBody.Builder()
                .setType(MultipartBody.FORM)
                .addFormDataPart("file", "logs.zip", requestBody)
                .build()

            val request = Request.Builder()
                .url("$baseUrl/upload")
                .post(multipartBody)
                .build()

            client.newCall(request).execute().use { response ->
                val responseBody = response.body?.string().orEmpty()
                if (!response.isSuccessful) {
                    return@withContext LogUploadResult(
                        success = false,
                        message = if (responseBody.isNotBlank()) responseBody else "HTTP ${response.code}"
                    )
                }

                val message = parseMessage(responseBody)
                return@withContext LogUploadResult(
                    success = true,
                    message = message.ifBlank { "上传成功" }
                )
            }
        } catch (e: IOException) {
            return@withContext LogUploadResult(
                success = false,
                message = e.message ?: "网络错误"
            )
        }
    }

    private fun collectTextFiles(rootDir: File): List<File> {
        return rootDir.walkTopDown()
            .filter { file -> file.isFile && file.extension.equals("txt", ignoreCase = true) }
            .sortedBy { file -> file.relativeTo(rootDir).path }
            .toList()
    }

    private fun createZipBytes(rootDir: File, files: List<File>): ByteArray {
        val buffer = ByteArrayOutputStream()
        ZipOutputStream(buffer).use { zip ->
            files.forEach { file ->
                val relativePath = file.relativeTo(rootDir).path.replace(File.separatorChar, '/')
                zip.putNextEntry(ZipEntry(relativePath))
                file.inputStream().use { input ->
                    input.copyTo(zip)
                }
                zip.closeEntry()
            }
        }
        return buffer.toByteArray()
    }

    private fun parseMessage(responseBody: String): String {
        if (responseBody.isBlank()) return ""
        return try {
            val json = JSONObject(responseBody)
            json.optString("message", json.optString("error", responseBody))
        } catch (_: JSONException) {
            responseBody
        }
    }
}
