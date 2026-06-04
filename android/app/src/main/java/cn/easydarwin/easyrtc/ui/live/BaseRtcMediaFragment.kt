package cn.easydarwin.easyrtc.ui.live

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import androidx.fragment.app.Fragment
import androidx.lifecycle.MutableLiveData
import cn.easyrtc.media.MediaSession
import cn.easyrtc.model.VideoEncodeConfig
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easyrtc.R
import kotlin.math.roundToInt

abstract class BaseRtcMediaFragment : Fragment() {
    protected lateinit var session: MediaSession
        private set

    private val cameraErrorEvent = MutableLiveData<Int?>()
    private val mainHandler = Handler(Looper.getMainLooper())

    override fun onViewCreated(view: android.view.View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        cameraErrorEvent.observe(viewLifecycleOwner) { error ->
            if (error == null) return@observe
            cameraErrorEvent.value = null
            mainHandler.post {
                if (!isAdded) return@post
                parentFragmentManager.beginTransaction()
                    .detach(this@BaseRtcMediaFragment)
                    .commitNowAllowingStateLoss()
                parentFragmentManager.beginTransaction()
                    .attach(this@BaseRtcMediaFragment)
                    .commitNowAllowingStateLoss()
            }
        }
        session = MediaSession().apply {
            create()
            setDeviceId(SPUtil.getInstance().rtcUserUUID)
            setCameraErrorListener { error ->
                cameraErrorEvent.postValue(error)
            }
        }.also {
            AppLogStore.appendCritical("BaseRtcMediaFragment", "MediaSession created: fragment=${this::class.java.simpleName}")
            onMediaSessionCreated(it)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        AppLogStore.appendCritical("BaseRtcMediaFragment", "releasing MediaSession for ${this::class.java.simpleName}")
        session.connectionState.removeObservers(viewLifecycleOwner)
        session.dataChannel.removeObservers(viewLifecycleOwner)
        session.remoteVideoSize.removeObservers(viewLifecycleOwner)
        onMediaSessionReleasing(session)
        session.setCameraErrorListener(null)
        session.release()
    }

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

    fun resetLocalPreview(fillParent: Boolean = false){
        view?.post {
            val v = view ?: return@post
            val local_preview_ = v.findViewById<View>(cn.easydarwin.easyrtc.R.id.local_preview_)
            val p = local_preview_.parent as View
            val width = if (fillParent) p.width else (114 * resources.displayMetrics.density).roundToInt()
            val config = getVideoEncodeConfig()
            local_preview_.layoutParams.width = width
            local_preview_.layoutParams.height = width *  config.getWidth() / config.getHeight()
            local_preview_.requestLayout()
        }
    }
}
