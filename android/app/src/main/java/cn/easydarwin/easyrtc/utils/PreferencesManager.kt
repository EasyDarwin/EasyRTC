package cn.easydarwin.easyrtc.utils

import android.content.Context
import android.content.SharedPreferences

class PreferencesManager private constructor(context: Context) {

    companion object {
        @Volatile
        private var INSTANCE: PreferencesManager? = null

        fun getInstance(context: Context): PreferencesManager {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: PreferencesManager(context.applicationContext).also {
                    INSTANCE = it
                }
            }
        }
    }

    private val sharedPreferences: SharedPreferences =
        context.getSharedPreferences("user_preferences", Context.MODE_PRIVATE)

    fun saveLogiinStatus(isLoggedIn: Boolean, username: String, token: String?, serverIp: String = "", serverPort: String = "") {
        with(sharedPreferences.edit()) {
            putBoolean("isLoggedIn", isLoggedIn)
            putString("username", username)
            putString("authToken", token)
            putString("serverIp", serverIp)
            putString("serverPort", serverPort)
            putLong("loginTime", System.currentTimeMillis()) // 保存登录时间
            apply()
        }
    }

    fun getLoginInfo(): LoginInfo {
        return LoginInfo(
            isLoggedIn = sharedPreferences.getBoolean("isLoggedIn", false),
            username = sharedPreferences.getString("username", "") ?: "",
            token = sharedPreferences.getString("authToken", "") ?: "",
            serverIp = sharedPreferences.getString("serverIp", "") ?: "",
            serverPort = sharedPreferences.getString("serverPort", "") ?: "",
            loginTime = sharedPreferences.getLong("loginTime", 0)
        )
    }

    fun clearLoginInfo() {
        with(sharedPreferences.edit()) {
            putBoolean("isLoggedIn", false)
            putString("authToken", "")
            apply()
        }
    }

    fun isTokenExpired(expiryTimeMillis: Long = 24 * 60 * 60 * 1000): Boolean {
        val loginTime = sharedPreferences.getLong("loginTime", 0)
        if (loginTime == 0L) return true

        val currentTime = System.currentTimeMillis()
        return (currentTime - loginTime) > expiryTimeMillis
    }
}

data class LoginInfo(
    val isLoggedIn: Boolean,
    val username: String,
    val token: String,
    val serverIp: String,
    val serverPort: String,
    val loginTime: Long
)