package cn.easydarwin.easyrtc.fragment

import android.os.Bundle
import android.media.MediaCodecList
import android.media.MediaCodecInfo
import android.text.InputType
import android.widget.Toast
import androidx.preference.EditTextPreference
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.repository.LogUploadRepository
import cn.easydarwin.easyrtc.ui.settings.SettingsValidator
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easyrtc.media.MediaSession
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch

class SettingFragment : PreferenceFragmentCompat() {

    private val logUploadRepository = LogUploadRepository()
    private var uploadLogsPreference: Preference? = null

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.settings_preferences, rootKey)

        // If device doesn't support HEVC encoding, force H264 and disable the selector
        if (!isHevcEncoderSupported()) {
            SPUtil.getInstance().hevcCodec = 0
            findPreference<ListPreference>(KEY_VIDEO_CODEC)?.apply {
                isEnabled = false
                summary = "${entries[0]} (HEVC not supported)"
            }
        }

        bindDeviceIdPreference()
        bindIndexedListPreference(
            key = KEY_VIDEO_CODEC,
            entriesResId = R.array.videocodearr,
            currentIndex = SPUtil.getInstance().hevcCodec,
        ) { SPUtil.getInstance().hevcCodec = it }
        bindIndexedListPreference(
            key = KEY_VIDEO_RESOLUTION,
            entriesResId = R.array.resolutionarr,
            currentIndex = SPUtil.getInstance().videoResolution,
        ) { SPUtil.getInstance().videoResolution = it }
        bindIndexedListPreference(
            key = KEY_VIDEO_FRAME_RATE,
            entriesResId = R.array.frameratearr,
            currentIndex = SPUtil.getInstance().frameRate,
        ) {
            SPUtil.getInstance().frameRate = it
            updateGopSummary()
        }
        bindIndexedListPreference(
            key = KEY_VIDEO_BIT_RATE,
            entriesResId = R.array.videocoderatearr,
            currentIndex = SPUtil.getInstance().bitRateKbps,
        ) { SPUtil.getInstance().bitRateKbps = it }
        bindGopPreference()
        bindIndexedListPreference(
            key = KEY_AUDIO_CODEC,
            entriesResId = R.array.audiocodearr,
            currentIndex = SPUtil.getInstance().aacCodec,
        ) { SPUtil.getInstance().aacCodec = it }
        bindIndexedListPreference(
            key = KEY_AUDIO_SAMPLE_RATE,
            entriesResId = R.array.samplingratearr,
            currentIndex = SPUtil.getInstance().samplingRate,
        ) { SPUtil.getInstance().samplingRate = it }
        bindIndexedListPreference(
            key = KEY_AUDIO_CHANNEL,
            entriesResId = R.array.audiochannelarr,
            currentIndex = SPUtil.getInstance().audioChannel,
        ) { SPUtil.getInstance().audioChannel = it }
        bindIndexedListPreference(
            key = KEY_AUDIO_BIT_RATE,
            entriesResId = R.array.audiocoderatearr,
            currentIndex = SPUtil.getInstance().audioCodeRate,
        ) { SPUtil.getInstance().audioCodeRate = it }

        uploadLogsPreference = findPreference(KEY_UPLOAD_LOGS)
        uploadLogsPreference?.setOnPreferenceClickListener {
            reportLogs()
            true
        }

        findPreference<Preference>(KEY_ABOUT_VERSION)?.summary = getAppVersionNameOrUnknown()
        findPreference<Preference>(KEY_SDK_VERSION)?.summary = try { MediaSession.getSdkVersion() } catch (_: Exception) { "N/A" }
    }

    override fun onDestroy() {
        super.onDestroy()
        uploadLogsPreference = null
    }

    private fun bindDeviceIdPreference() {
        val preference = findPreference<EditTextPreference>(KEY_DEVICE_ID) ?: return
        val storedValue = SPUtil.getInstance().rtcUserUUID
        val validated = SettingsValidator.validateDeviceId(storedValue)
        preference.text = validated.normalized
        preference.summary = validated.normalized.ifBlank { getString(R.string.settings_device_id_helper) }
        preference.setOnBindEditTextListener { editText ->
            editText.inputType = InputType.TYPE_CLASS_TEXT
        }
        preference.setOnPreferenceChangeListener { pref, newValue ->
            val result = SettingsValidator.validateDeviceId(newValue.toString())
            if (!result.isValid) {
                Toast.makeText(requireContext(), getString(result.errorMessageResId ?: R.string.settings_device_id_error_format), Toast.LENGTH_SHORT).show()
                return@setOnPreferenceChangeListener false
            }
            SPUtil.getInstance().rtcUserUUID = result.normalized
            (pref as EditTextPreference).text = result.normalized
            pref.summary = result.normalized
            true
        }
    }

    private fun bindIndexedListPreference(
        key: String,
        entriesResId: Int,
        currentIndex: Int,
        onValueChanged: (Int) -> Unit,
    ) {
        val preference = findPreference<ListPreference>(key) ?: return
        val entries = resources.getStringArray(entriesResId)
        val entryValues = Array(entries.size) { index -> index.toString() }
        val safeIndex = currentIndex.coerceIn(entryValues.indices)

        preference.entries = entries
        preference.entryValues = entryValues
        preference.value = safeIndex.toString()
        preference.summary = entries[safeIndex]
        preference.setOnPreferenceChangeListener { pref, newValue ->
            val index = newValue.toString().toIntOrNull() ?: return@setOnPreferenceChangeListener false
            if (index !in entries.indices) return@setOnPreferenceChangeListener false
            onValueChanged(index)
            (pref as ListPreference).summary = entries[index]
            true
        }
    }

    private fun bindGopPreference() {
        val preference = findPreference<ListPreference>(KEY_VIDEO_GOP) ?: return
        val entries = resources.getStringArray(R.array.goparr)
        val entryValues = Array(entries.size) { index -> index.toString() }
        val safeIndex = SPUtil.getInstance().gopInterval.coerceIn(entryValues.indices)

        preference.entries = entries
        preference.entryValues = entryValues
        preference.value = safeIndex.toString()
        preference.setOnPreferenceChangeListener { pref, newValue ->
            val index = newValue.toString().toIntOrNull() ?: return@setOnPreferenceChangeListener false
            if (index !in entries.indices) return@setOnPreferenceChangeListener false
            SPUtil.getInstance().gopInterval = index
            updateGopSummary()
            true
        }
        updateGopSummary()
    }

    private fun updateGopSummary() {
        val preference = findPreference<ListPreference>(KEY_VIDEO_GOP) ?: return
        val iFrameInterval = SPUtil.getInstance().getVideoGopInterval()
        val fps = SPUtil.getInstance().getVideoFrameRate()
        val gop = fps * iFrameInterval
        preference.summary = "${iFrameInterval}s (GOP ${gop})"
    }

    private fun reportLogs() {
        val preference = uploadLogsPreference ?: return
        val appContext = requireContext().applicationContext
        AppLogStore.appendCritical("SettingFragment", "reportLogs: triggered by user")
        preference.isEnabled = false
        lifecycleScope.launch {
            val result = logUploadRepository.uploadLogs(appContext.filesDir)
            if (result.success) {
                AppLogStore.appendCritical("SettingFragment", "reportLogs: upload success message=${result.message}")
                Toast.makeText(appContext, appContext.getString(R.string.upload_log_success), Toast.LENGTH_SHORT).show()
            } else {
                AppLogStore.appendCritical("SettingFragment", "reportLogs: upload failed message=${result.message}")
                Toast.makeText(
                    appContext,
                    "${appContext.getString(R.string.upload_log_failed)}: ${result.message}",
                    Toast.LENGTH_SHORT
                ).show()
            }
            preference.isEnabled = true
        }
    }

    private fun getAppVersionNameOrUnknown(): String {
        return try {
            val context = requireContext()
            val packageInfo = if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.TIRAMISU) {
                context.packageManager.getPackageInfo(
                    context.packageName,
                    android.content.pm.PackageManager.PackageInfoFlags.of(0),
                )
            } else {
                @Suppress("DEPRECATION")
                context.packageManager.getPackageInfo(context.packageName, 0)
            }
            packageInfo.versionName?.takeIf { it.isNotBlank() }
                ?: getString(R.string.about_version_unknown)
        } catch (_: android.content.pm.PackageManager.NameNotFoundException) {
            getString(R.string.about_version_unknown)
        }
    }

    private fun isHevcEncoderSupported(): Boolean {
        return try {
            val codecList = MediaCodecList(MediaCodecList.REGULAR_CODECS)
            codecList.codecInfos.any { info ->
                info.isEncoder && info.supportedTypes.contains("video/hevc")
            }
        } catch (_: Exception) {
            false
        }
    }

    private companion object {
        const val KEY_DEVICE_ID = "pref_device_id"
        const val KEY_VIDEO_CODEC = "pref_video_codec"
        const val KEY_VIDEO_RESOLUTION = "pref_video_resolution"
        const val KEY_VIDEO_FRAME_RATE = "pref_video_frame_rate"
        const val KEY_VIDEO_BIT_RATE = "pref_video_bit_rate"
        const val KEY_VIDEO_GOP = "pref_video_gop"
        const val KEY_AUDIO_CODEC = "pref_audio_codec"
        const val KEY_AUDIO_SAMPLE_RATE = "pref_audio_sample_rate"
        const val KEY_AUDIO_CHANNEL = "pref_audio_channel"
        const val KEY_AUDIO_BIT_RATE = "pref_audio_bit_rate"
        const val KEY_UPLOAD_LOGS = "pref_upload_logs"
        const val KEY_ABOUT_VERSION = "pref_about_version"
        const val KEY_SDK_VERSION = "pref_sdk_version"
    }
}
