package cn.easydarwin.easyrtc.fragment

import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.TextView
import androidx.core.view.isVisible
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.ui.settings.SettingsValidator
import cn.easydarwin.easyrtc.utils.CommonSpinnerHelper
import cn.easydarwin.easyrtc.utils.SPUtil

class SettingFragment : Fragment() {

    private var mEtDeviceId: EditText? = null
    private var tvDeviceIdError: TextView? = null

    // 视频设置相关视图
    private var tvVideoEncoded: TextView? = null
    private var tvVideoResolution: TextView? = null
    private var tvFrameRate: TextView? = null
    private var tvVideoCodeRate: TextView? = null

    // 音频设置相关视图
    private var tvAudioEncoded: TextView? = null
    private var tvAudioSampleRate: TextView? = null
    private var tvAudioChannel: TextView? = null
    private var tvAudioRate: TextView? = null

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_setting, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        initViews(view)
        initDeviceId()
        initVideoSettings(view)
        initAudioSettings(view)
    }

    private fun initViews(view: View) {
        mEtDeviceId = view.findViewById(R.id.et_device_id)
        tvDeviceIdError = view.findViewById(R.id.tv_device_id_error)

        // 视频设置视图
        tvVideoEncoded = view.findViewById(R.id.tv_video_encoded)
        tvVideoResolution = view.findViewById(R.id.tv_video_resolution)
        tvFrameRate = view.findViewById(R.id.tv_frame_rate)
        tvVideoCodeRate = view.findViewById(R.id.tv_video_code_rate)

        // 音频设置视图
        tvAudioEncoded = view.findViewById(R.id.tv_audio_encoded)
        tvAudioSampleRate = view.findViewById(R.id.tv_audio_sample_rate)
        tvAudioChannel = view.findViewById(R.id.tv_audio_channel)
        tvAudioRate = view.findViewById(R.id.tv_audio_rate)


        mEtDeviceId?.doOnTextChanged { text, _, _, _ ->
            val rawValue = text?.toString().orEmpty()
            val validated = SettingsValidator.validateDeviceId(rawValue)
            tvDeviceIdError?.isVisible = !validated.isValid
            tvDeviceIdError?.text = validated.errorMessageResId?.let { getString(it) }.orEmpty()
            if (validated.isValid) {
                SPUtil.getInstance().rtcUserUUID = validated.normalized
            }
        }
    }

    private fun initDeviceId() {
        val storedValue = SPUtil.getInstance().rtcUserUUID
        val validated = SettingsValidator.validateDeviceId(storedValue)
        mEtDeviceId?.setText(validated.normalized)
        tvDeviceIdError?.isVisible = !validated.isValid
        tvDeviceIdError?.text = validated.errorMessageResId?.let { getString(it) }.orEmpty()
        if (validated.isValid && validated.normalized != storedValue) {
            SPUtil.getInstance().rtcUserUUID = validated.normalized
        }
    }

    private fun initVideoSettings(view: View) {
        // 视频编码 - 默认选中第一个 (H264)
        view.findViewById<View>(R.id.ll_video_encoded)?.let { anchorView ->
            val valueView = tvVideoEncoded ?: return@let
            CommonSpinnerHelper.initSpinner(
                requireContext(), anchorView, valueView, R.array.videocodearr, SPUtil.getInstance().hevcCodec // 默认选中第一个
            ) { selectedValue, position ->
                Log.d(TAG, "视频编码选择: $selectedValue, 位置: $position")
                SPUtil.getInstance().hevcCodec = position
            }
        }

        // 视频分辨率 - 默认选中第一个 (1920*1080)
        view.findViewById<View>(R.id.ll_video_resolution)?.let { anchorView ->
            val valueView = tvVideoResolution ?: return@let
            CommonSpinnerHelper.initSpinner(
                requireContext(), anchorView, valueView, R.array.resolutionarr, SPUtil.getInstance().videoResolution // 默认选中第一个
            ) { selectedValue, position ->
                Log.d(TAG, "分辨率选择: $selectedValue, 位置: $position")
                SPUtil.getInstance().videoResolution = position
            }
        }

        // 帧率 - 默认选中第三个 (20)
        view.findViewById<View>(R.id.ll_frame_rate)?.let { anchorView ->
            val valueView = tvFrameRate ?: return@let
            CommonSpinnerHelper.initSpinner(
                requireContext(), anchorView, valueView, R.array.frameratearr, SPUtil.getInstance().frameRate // 默认选中第三个 (20fps)
            ) { selectedValue, position ->
                Log.d(TAG, "帧率选择: $selectedValue, 位置: $position")
                SPUtil.getInstance().frameRate = position
            }
        }

        // 视频码率 - 默认选中第三个 (4096)
        view.findViewById<View>(R.id.ll_video_code_rate)?.let { anchorView ->
            val valueView = tvVideoCodeRate ?: return@let
            CommonSpinnerHelper.initSpinner(
                requireContext(), anchorView, valueView, R.array.videocoderatearr, SPUtil.getInstance().bitRateKbps// 默认选中第三个 (4096)
            ) { selectedValue, position ->
                Log.d(TAG, "视频码率选择: $selectedValue, 位置: $position")
                SPUtil.getInstance().bitRateKbps = position
            }
        }
    }


    private fun initAudioSettings(view: View) {
        // 音频编码
        view.findViewById<View>(R.id.ll_audio_encoded)?.let { anchorView ->
            val valueView = tvAudioEncoded ?: return@let
            CommonSpinnerHelper.initSpinner(
                requireContext(), anchorView, valueView, R.array.audiocodearr, SPUtil.getInstance().aacCodec
            ) { selectedValue, position ->
                Log.d(TAG, "音频编码选择: $selectedValue, 位置: $position")
                SPUtil.getInstance().aacCodec = position
            }
        }

        // 采样率
        view.findViewById<View>(R.id.ll_audio_sample_rate)?.let { anchorView ->
            val valueView = tvAudioSampleRate ?: return@let
            CommonSpinnerHelper.initSpinner(
                requireContext(), anchorView, valueView, R.array.samplingratearr, SPUtil.getInstance().samplingRate
            ) { selectedValue, position ->
                Log.d(TAG, "采样率选择: $selectedValue, 位置: $position")
                SPUtil.getInstance().samplingRate = position
            }
        }

        // 声道
        view.findViewById<View>(R.id.ll_audio_channel)?.let { anchorView ->
            val valueView = tvAudioChannel ?: return@let
            CommonSpinnerHelper.initSpinner(
                requireContext(), anchorView, valueView, R.array.audiochannelarr, SPUtil.getInstance().audioChannel
            ) { selectedValue, position ->
                Log.d(TAG, "声道选择: $selectedValue, 位置: $position")
                SPUtil.getInstance().audioChannel = position
            }
        }

        // 音频码率
        view.findViewById<View>(R.id.ll_audio_rate)?.let { anchorView ->
            val valueView = tvAudioRate ?: return@let
            CommonSpinnerHelper.initSpinner(
                requireContext(), anchorView, valueView, R.array.audiocoderatearr, SPUtil.getInstance().audioCodeRate
            ) { selectedValue, position ->
                Log.d(TAG, "音频码率选择: $selectedValue, 位置: $position")
                SPUtil.getInstance().audioCodeRate = position
            }
        }
    }

    override fun onPause() {
        super.onPause()
        Log.d(TAG, "onPause")
    }

    override fun onResume() {
        super.onResume()
        Log.d(TAG, "onResume")
    }

    override fun onDestroyView() {
        super.onDestroyView()
        mEtDeviceId = null
        tvDeviceIdError = null
        tvVideoEncoded = null
        tvVideoResolution = null
        tvFrameRate = null
        tvVideoCodeRate = null
        tvAudioEncoded = null
        tvAudioSampleRate = null
        tvAudioChannel = null
        tvAudioRate = null
        Log.d(TAG, "onDestroyView")
    }

    companion object {
        private const val TAG = "SettingFragment"
    }
}
