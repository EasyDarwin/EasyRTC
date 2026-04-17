package cn.easydarwin.easyrtc.core.result

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test

class AppResultTest {

    @Test
    fun success_wraps_value() {
        val result: AppResult<Int> = AppResult.Success(42)

        assertTrue(result is AppResult.Success)
        when (result) {
            is AppResult.Success -> assertEquals(42, result.data)
            else -> fail("Expected AppResult.Success but got ${result::class.simpleName}")
        }
    }

    @Test
    fun error_wraps_message() {
        val result: AppResult<Nothing> = AppResult.Error("network error")

        assertTrue(result is AppResult.Error)
        when (result) {
            is AppResult.Error -> assertEquals("network error", result.message)
            else -> fail("Expected AppResult.Error but got ${result::class.simpleName}")
        }
    }

    @Test
    fun error_preserves_cause() {
        val cause = IllegalStateException("boom")
        val result: AppResult<Nothing> = AppResult.Error("network error", cause)

        when (result) {
            is AppResult.Error -> assertEquals(cause, result.cause)
            else -> fail("Expected AppResult.Error but got ${result::class.simpleName}")
        }
    }
}
