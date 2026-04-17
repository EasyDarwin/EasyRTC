package cn.easydarwin.easyrtc.ui.settings

import androidx.annotation.StringRes
import cn.easydarwin.easyrtc.R

object SettingsValidator {

    private val UUID_REGEX = Regex("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")

    data class ValidationResult(
        val isValid: Boolean,
        val normalized: String,
        @StringRes val errorMessageResId: Int? = null,
    )

    fun validateDeviceId(input: String): ValidationResult {
        val normalized = input.trim()
        if (normalized.isEmpty()) {
            return ValidationResult(
                isValid = false,
                normalized = normalized,
                errorMessageResId = R.string.settings_device_id_error_empty,
            )
        }
        if (!UUID_REGEX.matches(normalized)) {
            return ValidationResult(
                isValid = false,
                normalized = normalized,
                errorMessageResId = R.string.settings_device_id_error_format,
            )
        }
        return ValidationResult(isValid = true, normalized = normalized)
    }
}
