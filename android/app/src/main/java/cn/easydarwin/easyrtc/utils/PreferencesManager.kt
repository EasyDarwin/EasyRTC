package cn.easydarwin.easyrtc.utils

import android.content.Context
import android.content.SharedPreferences

class PreferencesManager private constructor(private val sharedPreferences: SharedPreferences) {

    private constructor(context: Context) : this(
        context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
    )

    companion object {
        private const val PREF_NAME = "user_preferences"
        private const val KEY_IS_LOGGED_IN = "isLoggedIn"
        private const val KEY_USERNAME = "username"
        private const val KEY_AUTH_TOKEN = "authToken"
        private const val KEY_SERVER_IP = "serverIp"
        private const val KEY_SERVER_PORT = "serverPort"
        private const val KEY_LOGIN_TIME = "loginTime"

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

    @Deprecated("Use saveLoginStatus")
    fun saveLogiinStatus(isLoggedIn: Boolean, username: String, token: String?, serverIp: String = "", serverPort: String = "") {
        saveLoginStatus(isLoggedIn, username, token, serverIp, serverPort)
    }

    fun saveLoginStatus(isLoggedIn: Boolean, username: String, token: String?, serverIp: String = "", serverPort: String = "") {
        with(sharedPreferences.edit()) {
            putBoolean(KEY_IS_LOGGED_IN, isLoggedIn)
            putString(KEY_USERNAME, username)
            putString(KEY_AUTH_TOKEN, token)
            putString(KEY_SERVER_IP, serverIp)
            putString(KEY_SERVER_PORT, serverPort)
            putLong(KEY_LOGIN_TIME, System.currentTimeMillis())
            apply()
        }
    }

    fun getLoginInfo(): LoginInfo {
        return LoginInfo(
            isLoggedIn = sharedPreferences.getBoolean(KEY_IS_LOGGED_IN, false),
            username = sharedPreferences.getString(KEY_USERNAME, "") ?: "",
            token = sharedPreferences.getString(KEY_AUTH_TOKEN, "") ?: "",
            serverIp = sharedPreferences.getString(KEY_SERVER_IP, "") ?: "",
            serverPort = sharedPreferences.getString(KEY_SERVER_PORT, "") ?: "",
            loginTime = sharedPreferences.getLong(KEY_LOGIN_TIME, 0)
        )
    }

    fun clearLoginInfo() {
        with(sharedPreferences.edit()) {
            putBoolean(KEY_IS_LOGGED_IN, false)
            putString(KEY_USERNAME, "")
            putString(KEY_AUTH_TOKEN, "")
            putString(KEY_SERVER_IP, "")
            putString(KEY_SERVER_PORT, "")
            putLong(KEY_LOGIN_TIME, 0L)
            apply()
        }
    }

    fun isTokenExpired(expiryTimeMillis: Long = 24 * 60 * 60 * 1000): Boolean {
        val loginTime = sharedPreferences.getLong(KEY_LOGIN_TIME, 0)
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
