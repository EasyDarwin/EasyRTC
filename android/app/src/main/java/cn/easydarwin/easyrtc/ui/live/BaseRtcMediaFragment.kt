package cn.easydarwin.easyrtc.ui.live

import android.os.Bundle
import android.os.Looper
import android.view.Surface
import androidx.fragment.app.Fragment
import cn.easyrtc.media.MediaSession
import cn.easyrtc.model.VideoEncodeConfig
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil

abstract class BaseRtcMediaFragment : Fragment() {
    protected lateinit var session: MediaSession
        private set

    override fun onViewCreated(view: android.view.View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        session = MediaSession().apply {
            create()
            setDeviceId(SPUtil.getInstance().rtcUserUUID)
        }.also {
            AppLogStore.appendCritical("BaseRtcMediaFragment", "MediaSession created: fragment=${this::class.java.simpleName}")
            onMediaSessionCreated(it)
        }
    }

    override fun onResume() {
        super.onResume()
        if (session.hasCameraError) {
            val surface = getPreviewSurfaceForRestart()
            surface?.let {
                AppLogStore.appendCritical(
                    "BaseRtcMediaFragment",
                    "Camera restart after device error, fragment=${this::class.java.simpleName}")
                session.setupVideoEncoder(getVideoEncodeConfig())
                val result = session.startPreview(it)
                if (result == 0) {
                    AppLogStore.appendCritical("BaseRtcMediaFragment", "Camera restarted successfully")
                } else {
                    AppLogStore.appendCritical("BaseRtcMediaFragment", "Camera restart failed: $result")
                }
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        AppLogStore.appendCritical("BaseRtcMediaFragment", "releasing MediaSession for ${this::class.java.simpleName}")
        session.connectionState.removeObservers(viewLifecycleOwner)
        session.dataChannel.removeObservers(viewLifecycleOwner)
        session.remoteVideoSize.removeObservers(viewLifecycleOwner)
        onMediaSessionReleasing(session)
        session.release()
    }

    protected open fun getPreviewSurfaceForRestart(): Surface? = null

    protected open fun onMediaSessionCreated(session: MediaSession) {}

    protected open fun onMediaSessionReleasing(session: MediaSession) {}

    protected fun getVideoEncodeConfig(): VideoEncodeConfig {
        val useHevc = SPUtil.getInstance().getIsHevc()
        val cameraId = SPUtil.getInstance().cameraId
        val resolution = SPUtil.getInstance().getVideoResolution()
        val bitRate = SPUtil.getInstance().getVideoBitRateKbps()
        val frameRate = SPUtil.getInstance().getVideoFrameRate()
        val iFrameInterval = SPUtil.getInstance().getVideoGopInterval()
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
            bitRate = bitRate,
            iFrameInterval = iFrameInterval
        )
    }

    protected fun runOnMainThread(action: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            action()
        } else {
            activity?.runOnUiThread(action) ?: view?.post(action)
        }
    }
}
