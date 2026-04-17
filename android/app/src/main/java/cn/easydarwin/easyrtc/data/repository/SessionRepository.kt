package cn.easydarwin.easyrtc.data.repository

import cn.easydarwin.easyrtc.core.result.AppResult
import cn.easydarwin.easyrtc.repository.LoginRepository
import cn.easydarwin.easyrtc.repository.LoginResult

class SessionRepository(
    private val loginRepository: LoginRepository = LoginRepository()
) {

    suspend fun login(
        ip: String,
        port: String,
        username: String,
        password: String
    ): AppResult<LoginResult> = loginRepository.loginResult(ip, port, username, password)
}
