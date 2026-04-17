package cn.easydarwin.easyrtc.data.repository

import cn.easydarwin.easyrtc.core.result.AppResult
import cn.easydarwin.easyrtc.repository.LoginRepository
import cn.easydarwin.easyrtc.repository.LoginResult
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SessionRepositoryTest {

    @Test
    fun login_returns_success_when_login_repository_succeeds() = runBlocking {
        val expected = LoginResult(success = true, message = "ok", token = "token-1", expired = 123L)
        val repository = SessionRepository(
            loginRepository = object : LoginRepository() {
                override suspend fun loginResult(
                    ip: String,
                    port: String,
                    username: String,
                    password: String
                ): AppResult<LoginResult> {
                    return AppResult.Success(expected)
                }
            }
        )

        val result = repository.login("127.0.0.1", "443", "u", "p")

        assertTrue(result is AppResult.Success)
        val data = (result as AppResult.Success).data
        assertEquals(expected, data)
    }

    @Test
    fun login_returns_error_when_login_repository_fails() = runBlocking {
        val repository = SessionRepository(
            loginRepository = object : LoginRepository() {
                override suspend fun loginResult(
                    ip: String,
                    port: String,
                    username: String,
                    password: String
                ): AppResult<LoginResult> {
                    return AppResult.Error("auth failed")
                }
            }
        )

        val result = repository.login("127.0.0.1", "443", "u", "p")

        assertTrue(result is AppResult.Error)
        val error = result as AppResult.Error
        assertEquals("auth failed", error.message)
    }

    @Test
    fun login_keeps_error_cause_from_login_repository() = runBlocking {
        val cause = IllegalStateException("boom")
        val repository = SessionRepository(
            loginRepository = object : LoginRepository() {
                override suspend fun loginResult(
                    ip: String,
                    port: String,
                    username: String,
                    password: String
                ): AppResult<LoginResult> {
                    return AppResult.Error("auth failed", cause)
                }
            }
        )

        val result = repository.login("127.0.0.1", "443", "u", "p")

        assertTrue(result is AppResult.Error)
        val error = result as AppResult.Error
        assertEquals("auth failed", error.message)
        assertEquals(cause, error.cause)
    }
}
