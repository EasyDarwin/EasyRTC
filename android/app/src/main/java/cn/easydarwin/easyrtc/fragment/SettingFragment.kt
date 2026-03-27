package cn.easydarwin.easyrtc.fragment

import android.content.Context
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.TextView
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.utils.CommonSpinnerHelper
import cn.easydarwin.easyrtc.utils.SPUtil

class SettingFragment : Fragment() {

    private var mEtDeviceId: EditText? = null

    // 视频设置相关视图
    private lateinit var tvVideoEncoded: TextView
    private lateinit var tvVideoResolution: TextView
    private lateinit var tvFrameRate: TextView
    private lateinit var tvVideoCodeRate: TextView

    // 音频设置相关视图
    private lateinit var tvAudioEncoded: TextView
    private lateinit var tvAudioSampleRate: TextView
    private lateinit var tvAudioChannel: TextView
    private lateinit var tvAudioRate: TextView
    private lateinit var mContext: Context

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_setting, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        this.mContext = requireContext()
        initViews(view)
        initDeviceId()
        initVideoSettings(view)
        initAudioSettings(view)
    }

    private fun initViews(view: View) {
        mEtDeviceId = view.findViewById(R.id.et_device_id)

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
            SPUtil.getInstance().rtcUserUUID = text.toString()
        }
    }

    private fun initDeviceId() {
        mEtDeviceId?.setText(SPUtil.Companion.getInstance().rtcUserUUID)
    }

    private fun initVideoSettings(view: View) {
        // 视频编码 - 默认选中第一个 (H264)
        view.findViewById<View>(R.id.ll_video_encoded)?.let { anchorView ->
            CommonSpinnerHelper.initSpinner(
                mContext, anchorView, tvVideoEncoded, R.array.videocodearr, SPUtil.Companion.getInstance().hevcCodec // 默认选中第一个
            ) { selectedValue, position ->
                Log.d(TAG, "视频编码选择: $selectedValue, 位置: $position")
                SPUtil.Companion.getInstance().hevcCodec = position
            }
        }

        // 视频分辨率 - 默认选中第一个 (1920*1080)
        view.findViewById<View>(R.id.ll_video_resolution)?.let { anchorView ->
            CommonSpinnerHelper.initSpinner(
                mContext, anchorView, tvVideoResolution, R.array.resolutionarr, SPUtil.Companion.getInstance().videoResolution // 默认选中第一个
            ) { selectedValue, position ->
                Log.d(TAG, "分辨率选择: $selectedValue, 位置: $position")
                SPUtil.Companion.getInstance().videoResolution = position
            }
        }

        // 帧率 - 默认选中第三个 (20)
        view.findViewById<View>(R.id.ll_frame_rate)?.let { anchorView ->
            CommonSpinnerHelper.initSpinner(
                mContext, anchorView, tvFrameRate, R.array.frameratearr, SPUtil.Companion.getInstance().frameRate // 默认选中第三个 (20fps)
            ) { selectedValue, position ->
                Log.d(TAG, "帧率选择: $selectedValue, 位置: $position")
                SPUtil.Companion.getInstance().frameRate = position
            }
        }

        // 视频码率 - 默认选中第三个 (4096)
        view.findViewById<View>(R.id.ll_video_code_rate)?.let { anchorView ->
            CommonSpinnerHelper.initSpinner(
                mContext, anchorView, tvVideoCodeRate, R.array.videocoderatearr, SPUtil.Companion.getInstance().bitRateKbps// 默认选中第三个 (4096)
            ) { selectedValue, position ->
                Log.d(TAG, "视频码率选择: $selectedValue, 位置: $position")
                SPUtil.Companion.getInstance().bitRateKbps = position
            }
        }
    }


    private fun initAudioSettings(view: View) {
        // 音频编码
        view.findViewById<View>(R.id.ll_audio_encoded)?.let { anchorView ->
            CommonSpinnerHelper.initSpinner(
                mContext, anchorView, tvAudioEncoded, R.array.audiocodearr, 0
            ) { selectedValue, position ->
                Log.d(TAG, "音频编码选择: $selectedValue, 位置: $position")
            }
        }

        // 采样率
        view.findViewById<View>(R.id.ll_audio_sample_rate)?.let { anchorView ->
            CommonSpinnerHelper.initSpinner(
                mContext, anchorView, tvAudioSampleRate, R.array.samplingratearr, 0
            ) { selectedValue, position ->
                Log.d(TAG, "采样率选择: $selectedValue, 位置: $position")
            }
        }

        // 声道
        view.findViewById<View>(R.id.ll_audio_channel)?.let { anchorView ->
            CommonSpinnerHelper.initSpinner(
                mContext, anchorView, tvAudioChannel, R.array.audiochannelarr, 0
            ) { selectedValue, position ->
                Log.d(TAG, "声道选择: $selectedValue, 位置: $position")
            }
        }

        // 音频码率
        view.findViewById<View>(R.id.ll_audio_rate)?.let { anchorView ->
            CommonSpinnerHelper.initSpinner(
                mContext, anchorView, tvAudioRate, R.array.audiocoderatearr, 0
            ) { selectedValue, position ->
                Log.d(TAG, "音频码率选择: $selectedValue, 位置: $position")
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
        Log.d(TAG, "onDestroyView")
    }

    companion object {
        private const val TAG = "SettingFragment"
    }
}