package cn.easydarwin.easyrtc.modal

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import cn.easydarwin.easyrtc.repository.LoginRepository
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.net.InetSocketAddress
import java.net.Socket


class LoginViewModel() : ViewModel() {

    private val _loginState = MutableLiveData<LoginState>(LoginState.Idle)
    val loginState: LiveData<LoginState> = _loginState

    private val loginRepository = LoginRepository()

    fun login(ip: String, port: String, username: String, password: String) {
        // 验证输入
        if (ip.isEmpty() || port.isEmpty() || username.isEmpty() || password.isEmpty()) {
            _loginState.value = LoginState.Error("所有字段都必须填写")
            return
        }

        val portNum = port.toIntOrNull()
        if (portNum == null || portNum !in 1..65535) {
            _loginState.value = LoginState.Error("端口号必须在1-65535之间")
            return
        }

        viewModelScope.launch {
            _loginState.value = LoginState.Loading

            try {
                // 1. 先检查网络连接
                if (!checkNetworkConnection(ip, portNum)) {
                    _loginState.value = LoginState.Error("无法连接到服务器")
                    return@launch
                }

                // 2. 执行登录
                val result = withContext(Dispatchers.IO) {
                    loginRepository.login(ip, port, username, password)
                }

                if (result.success) {

                    _loginState.value = result.token?.let {
                        result.expired?.let { expired ->
                            LoginState.Success("登录成功", it, expired)
                        }
                    }
                } else {
                    _loginState.value = LoginState.Error(result.message ?: "登录失败")
                }

            } catch (e: Exception) {
                e.printStackTrace()
                _loginState.value = LoginState.Error("登录异常: ${e.message}")
            }
        }
    }

    private suspend fun checkNetworkConnection(ip: String, port: Int): Boolean {
        return withContext(Dispatchers.IO) {
            try {
                val socket = Socket()
                socket.connect(InetSocketAddress(ip, port), 3000)
                socket.close()
                true
            } catch (e: Exception) {
                false
            }
        }
    }
}

sealed class LoginState {
    object Idle : LoginState()
    object Loading : LoginState()
    data class Success(val message: String, val token: String, val expired: Long) : LoginState()
    data class Error(val message: String) : LoginState()
}