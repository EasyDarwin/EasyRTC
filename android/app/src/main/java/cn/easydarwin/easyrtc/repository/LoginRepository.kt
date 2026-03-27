package cn.easydarwin.easyrtc.repository

import cn.easydarwin.easyrtc.utils.CryptoUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.util.concurrent.TimeUnit
import javax.net.ssl.HostnameVerifier
import javax.net.ssl.X509TrustManager
import java.security.cert.X509Certificate
import javax.net.ssl.TrustManager
import javax.net.ssl.SSLContext

class LoginRepository {

    private val client = createOkHttpClient()

    private fun createOkHttpClient(): OkHttpClient {
        return try {
            // 创建信任所有证书的 TrustManager
            val trustAllCerts = arrayOf<TrustManager>(object : X509TrustManager {
                override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) {}
                override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {}
                override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
            })

            // 创建 SSLContext
            val sslContext = SSLContext.getInstance("SSL")
            sslContext.init(null, trustAllCerts, java.security.SecureRandom())

            // 创建自定义 HostnameVerifier，允许IP地址访问
            val hostnameVerifier = HostnameVerifier { hostname, session ->
                // 如果是IP地址格式，直接允许
                if (isValidIpAddress(hostname)) {
                    true
                } else {
                    // 对于域名，使用默认验证或自定义验证
                    // 这里简单允许所有，生产环境应该更严格
                    true
                    // 或者使用默认验证:
                    // HttpsURLConnection.getDefaultHostnameVerifier().verify(hostname, session)
                }
            }

            OkHttpClient.Builder()
                .sslSocketFactory(sslContext.socketFactory, trustAllCerts[0] as X509TrustManager)
                .hostnameVerifier(hostnameVerifier)
                .connectTimeout(10, TimeUnit.SECONDS)
                .readTimeout(10, TimeUnit.SECONDS)
                .writeTimeout(10, TimeUnit.SECONDS)
                .build()
        } catch (e: Exception) {
            // 如果SSL配置失败，回退到普通客户端
            OkHttpClient.Builder()
                .connectTimeout(10, TimeUnit.SECONDS)
                .readTimeout(10, TimeUnit.SECONDS)
                .writeTimeout(10, TimeUnit.SECONDS)
                .build()
        }
    }

    private fun isValidIpAddress(hostname: String): Boolean {
        return try {
            val parts = hostname.split(".")
            if (parts.size != 4) return false
            parts.all { part ->
                val num = part.toInt()
                num in 0..255
            }
        } catch (e: Exception) {
            false
        }
    }

    suspend fun login(ip: String, port: String, username: String, password: String): LoginResult {
        return withContext(Dispatchers.IO) {
            try {
                // 构建请求URL - 支持IP地址和域名
                val url = if (isValidIpAddress(ip)) {
                    "https://$ip:$port/login"  // 使用IP地址
                } else {
                    "https://$ip:$port/login"  // 使用域名
                }

                println("登录URL: $url")  // 调试信息

                val shaPassword = CryptoUtils.sha256Hash(password)

                // 构建请求体
                val json = JSONObject().apply {
                    put("username", username)
                    put("password", shaPassword)
                    // 根据实际API调整字段
                    put("captcha", "")
                    put("captcha_id", 0)
                }

                val requestBody = json.toString().toRequestBody(JSON_MEDIA_TYPE)

                val request = Request.Builder()
                    .url(url)
                    .post(requestBody)
                    .addHeader("Content-Type", "application/json")
                    .addHeader("Accept", "application/json")
                    .build()

                client.newCall(request).execute().use { response ->
                    val responseBody = response.body?.string()

                    println("响应码: ${response.code}")  // 调试信息
                    println("响应体: $responseBody")    // 调试信息

                    if (response.isSuccessful && responseBody != null) {
                        val jsonResponse = JSONObject(responseBody)

                        // 根据实际API响应格式调整
                        val success = when {
                            jsonResponse.has("code") -> jsonResponse.optInt("code") == 0
                            jsonResponse.has("success") -> jsonResponse.optBoolean("success", false)
                            else -> true  // 如果API没有明确返回success字段，假设成功
                        }

                        val message = jsonResponse.optString("message",
                            if (success) "登录成功" else "登录失败")
                        val token = jsonResponse.optString("token", null)
                        val expired = jsonResponse.optLong("expired_at")

                        return@withContext LoginResult(
                            success = success,
                            message = message,
                            token = token,
                            expired
                        )
                    } else {
                        val errorMsg = when (response.code) {
                            401 -> "认证失败，用户名或密码错误"
                            403 -> "访问被拒绝"
                            404 -> "API接口不存在"
                            500 -> "服务器内部错误"
                            else -> "服务器响应错误: ${response.code}"
                        }

                        return@withContext LoginResult(
                            success = false,
                            message = errorMsg
                        )
                    }
                }
            } catch (e: Exception) {
                e.printStackTrace()
                val errorMessage = when {
                    e.message?.contains("SSL") == true -> "SSL证书验证失败，请检查服务器配置"
                    e.message?.contains("timeout") == true -> "连接超时，请检查网络"
                    e.message?.contains("Unable to resolve host") == true -> "无法解析主机名"
                    else -> "连接失败: ${e.message ?: "未知错误"}"
                }

                return@withContext LoginResult(
                    success = false,
                    message = errorMessage
                )
            }
        }
    }

    companion object {
        private val JSON_MEDIA_TYPE = "application/json; charset=utf-8".toMediaType()
    }
}

data class LoginResult(
    val success: Boolean,
    val message: String = "",
    val token: String? = null,
    val expired: Long? = 0L,
)

// 扩展函数
private fun String.toMediaType() = toMediaTypeOrNull()!!