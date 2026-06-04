package cn.easydarwin.easyrtc.data.repository

import cn.easydarwin.easyrtc.utils.LoginInfo
import cn.easydarwin.easyrtc.utils.PreferencesManager

class SettingsRepository(
    private val preferencesManager: PreferencesManager
) {

    fun saveLoginStatus(
        isLoggedIn: Boolean,
        username: String,
        token: String?,
        serverIp: String = "",
        serverPort: String = ""
    ) {
        preferencesManager.saveLoginStatus(
            isLoggedIn = isLoggedIn,
            username = username,
            token = token,
            serverIp = serverIp,
            serverPort = serverPort
        )
    }

    fun getLoginInfo(): LoginInfo = preferencesManager.getLoginInfo()

    fun clearLoginInfo() {
        preferencesManager.clearLoginInfo()
    }

    fun isTokenExpired(expiryTimeMillis: Long = 24 * 60 * 60 * 1000): Boolean {
        return preferencesManager.isTokenExpired(expiryTimeMillis)
    }
}
