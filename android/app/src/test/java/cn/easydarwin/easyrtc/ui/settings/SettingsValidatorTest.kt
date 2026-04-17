package cn.easydarwin.easyrtc.ui.settings

import cn.easydarwin.easyrtc.R
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class SettingsValidatorTest {

    @Test
    fun validateDeviceId_whenEmpty_returnsError() {
        val result = SettingsValidator.validateDeviceId("   ")

        assertFalse(result.isValid)
        assertEquals("", result.normalized)
        assertEquals(R.string.settings_device_id_error_empty, result.errorMessageResId)
    }

    @Test
    fun validateDeviceId_whenNotUuid_returnsFormatError() {
        val result = SettingsValidator.validateDeviceId("device-001")

        assertFalse(result.isValid)
        assertEquals("device-001", result.normalized)
        assertEquals(R.string.settings_device_id_error_format, result.errorMessageResId)
    }

    @Test
    fun validateDeviceId_whenValidUuidWithSpace_returnsNormalizedValidValue() {
        val result = SettingsValidator.validateDeviceId("  550e8400-e29b-41d4-a716-446655440000  ")

        assertTrue(result.isValid)
        assertEquals("550e8400-e29b-41d4-a716-446655440000", result.normalized)
        assertNull(result.errorMessageResId)
    }

    @Test
    fun validateDeviceId_whenUppercaseUuid_isValid() {
        val result = SettingsValidator.validateDeviceId("550E8400-E29B-41D4-A716-446655440000")

        assertTrue(result.isValid)
        assertEquals("550E8400-E29B-41D4-A716-446655440000", result.normalized)
        assertNull(result.errorMessageResId)
    }

    @Test
    fun validateDeviceId_whenValidUuidWithTrailingWhitespace_returnsNormalizedValidValue() {
        val result = SettingsValidator.validateDeviceId("550e8400-e29b-41d4-a716-446655440000\t\n")

        assertTrue(result.isValid)
        assertEquals("550e8400-e29b-41d4-a716-446655440000", result.normalized)
        assertNull(result.errorMessageResId)
    }

    @Test
    fun validateDeviceId_whenMalformedUuidShape_returnsFormatError() {
        val result = SettingsValidator.validateDeviceId("550e8400-e29b-41d4-a716-44665544")

        assertFalse(result.isValid)
        assertEquals("550e8400-e29b-41d4-a716-44665544", result.normalized)
        assertEquals(R.string.settings_device_id_error_format, result.errorMessageResId)
    }
}
