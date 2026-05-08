package cn.easydarwin.easyrtc.ui.live

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Looper
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import cn.easyrtc.media.MediaSession
import cn.easyrtc.model.VideoEncodeConfig
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil

abstract class BaseRtcMediaFragment : Fragment() {
    protected lateinit var session: MediaSession

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        AppLogStore.appendCritical("BaseRtcMediaFragment", "onCreate: fragment=${this::class.java.simpleName}")
        session = MediaSession().apply {
            create()
            setDeviceId(SPUtil.getInstance().rtcUserUUID)
        }
        AppLogStore.appendCritical("BaseRtcMediaFragment", "MediaSession created: fragment=${this::class.java.simpleName}")
        onMediaSessionCreated(session)
    }

    protected open fun onMediaSessionCreated(session: MediaSession) {}

    protected open fun onMediaSessionReleasing(session: MediaSession) {}

    override fun onDestroy() {
        if (::session.isInitialized) {
            AppLogStore.appendCritical("BaseRtcMediaFragment", "onDestroy: releasing MediaSession for ${this::class.java.simpleName}")
            onMediaSessionReleasing(session)
            session.release()
        }
        super.onDestroy()
    }

    protected fun getVideoEncodeConfig(): VideoEncodeConfig {
        val useHevc = SPUtil.getInstance().getIsHevc()
        val cameraId = SPUtil.getInstance().cameraId
        val resolution = SPUtil.getInstance().getVideoResolution()
        val bitRate = SPUtil.getInstance().getVideoBitRateKbps()
        val frameRate = SPUtil.getInstance().getVideoFrameRate()
        AppLogStore.appendCritical(
            "BaseRtcMediaFragment",
            "VideoEncodeConfig: fragment=${this::class.java.simpleName} hevc=$useHevc cameraId=$cameraId resolution=${resolution.width}x${resolution.height} bitrate=$bitRate fps=$frameRate"
        )
        return VideoEncodeConfig(
            useHevc = useHevc,
            frameRate = frameRate,
            cameraId = cameraId,
            resolution = resolution,
            orientation = VideoEncodeConfig.ORIENTATION_90,
            bitRate = bitRate
        )
    }

    protected fun hasCameraPermission(): Boolean {
        return ContextCompat.checkSelfPermission(
            requireContext(),
            Manifest.permission.CAMERA
        ) == PackageManager.PERMISSION_GRANTED
    }

    protected fun runOnMainThread(action: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            action()
        } else {
            activity?.runOnUiThread(action) ?: view?.post(action)
        }
    }
}
