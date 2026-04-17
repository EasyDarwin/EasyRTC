package cn.easydarwin.easyrtc.repository

import android.util.Log
import cn.easydarwin.easyrtc.BuildConfig
import cn.easydarwin.easyrtc.core.result.AppResult
import cn.easydarwin.easyrtc.utils.CryptoUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.io.IOException
import java.net.SocketTimeoutException
import java.net.UnknownHostException
import java.util.concurrent.TimeUnit
import javax.net.ssl.HostnameVerifier
import java.security.cert.X509Certificate
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLException
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager

open class LoginRepository {

    private val tag = "LoginRepository"

    private val client = createOkHttpClient()

    private fun createOkHttpClient(): OkHttpClient {
        if (!BuildConfig.DEBUG) {
            return OkHttpClient.Builder()
                .connectTimeout(10, TimeUnit.SECONDS)
                .readTimeout(10, TimeUnit.SECONDS)
                .writeTimeout(10, TimeUnit.SECONDS)
                .build()
        }

        return try {
            // 创建信任所有证书的 TrustManager
            val trustAllCerts = arrayOf<TrustManager>(object : X509TrustManager {
                override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) {}
                override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {}
                override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
            })

            // 创建 SSLContext
            val sslContext = SSLContext.getInstance("TLS")
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

    open suspend fun login(ip: String, port: String, username: String, password: String): LoginResult {
        return withContext(Dispatchers.IO) {
            try {
                // 构建请求URL - 支持IP地址和域名
                val url = "https://$ip:$port/login"

                if (BuildConfig.DEBUG) {
                    Log.d(tag, "登录URL: $url")
                }

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

                    if (BuildConfig.DEBUG) {
                        Log.d(tag, "响应码: ${response.code}")
                        Log.d(tag, "响应体: $responseBody")
                    }

                    if (response.isSuccessful && responseBody != null) {
                        val jsonResponse = JSONObject(responseBody)

                        // 根据实际API响应格式调整
                        val success = when {
                            jsonResponse.has("code") -> jsonResponse.optInt("code") == 0
                            jsonResponse.has("success") -> jsonResponse.optBoolean("success", false)
                            else -> false
                        }

                        val message = jsonResponse.optString("message",
                            if (success) "登录成功" else "登录失败")
                        val token = jsonResponse.optString("token", "").takeIf { it.isNotBlank() }
                        val expired = if (jsonResponse.has("expired_at")) {
                            jsonResponse.optLong("expired_at")
                        } else {
                            null
                        }

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
            } catch (e: SSLException) {
                if (BuildConfig.DEBUG) {
                    Log.e(tag, "SSL exception during login", e)
                }
                return@withContext LoginResult(
                    success = false,
                    message = "SSL证书验证失败，请检查服务器配置"
                )
            } catch (e: SocketTimeoutException) {
                if (BuildConfig.DEBUG) {
                    Log.e(tag, "Timeout during login", e)
                }
                return@withContext LoginResult(
                    success = false,
                    message = "连接超时，请检查网络"
                )
            } catch (e: UnknownHostException) {
                if (BuildConfig.DEBUG) {
                    Log.e(tag, "Unknown host during login", e)
                }
                return@withContext LoginResult(
                    success = false,
                    message = "无法解析主机名"
                )
            } catch (e: IOException) {
                if (BuildConfig.DEBUG) {
                    Log.e(tag, "I/O exception during login", e)
                }
                return@withContext LoginResult(
                    success = false,
                    message = "连接失败: ${e.message ?: "网络错误"}"
                )
            } catch (e: Exception) {
                if (BuildConfig.DEBUG) {
                    Log.e(tag, "Unexpected exception during login", e)
                }
                return@withContext LoginResult(
                    success = false,
                    message = "连接失败: ${e.message ?: "未知错误"}"
                )
            }
        }
    }

    open suspend fun loginResult(
        ip: String,
        port: String,
        username: String,
        password: String
    ): AppResult<LoginResult> {
        return try {
            val result = login(ip, port, username, password)
            if (result.success) {
                AppResult.Success(result)
            } else {
                AppResult.Error(result.message.ifBlank { "登录失败" })
            }
        } catch (throwable: Throwable) {
            AppResult.Error(throwable.message ?: "登录失败", throwable)
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
    val expired: Long? = null,
)
