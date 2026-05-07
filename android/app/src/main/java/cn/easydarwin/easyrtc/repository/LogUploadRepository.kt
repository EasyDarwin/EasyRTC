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

    suspend fun uploadLogs(logText: String): LogUploadResult = withContext(Dispatchers.IO) {
        try {
            val zipBytes = createZipBytes(logText)
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

    private fun createZipBytes(logText: String): ByteArray {
        val buffer = ByteArrayOutputStream()
        ZipOutputStream(buffer).use { zip ->
            zip.putNextEntry(ZipEntry("local_logs.txt"))
            zip.write(logText.toByteArray(Charsets.UTF_8))
            zip.closeEntry()
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
