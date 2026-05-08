package cn.easydarwin.easyrtc.ui.whip

import android.graphics.Matrix
import android.graphics.SurfaceTexture
import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.util.Log
import android.view.LayoutInflater
import android.view.Surface
import android.view.TextureView
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.fragment.HomeFragment
import cn.easyrtc.EasyRTCCodec
import cn.easyrtc.EasyRTCPeerConnectionState
import cn.easyrtc.EasyRTCSdk
import cn.easyrtc.whip.WhipModule
import cn.easydarwin.easyrtc.ui.live.BaseRtcMediaFragment
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil

class WhipFragment : BaseRtcMediaFragment(), TextureView.SurfaceTextureListener, EasyRTCSdk.EasyRTCEventListener {

    companion object {
        private const val TAG = "WhipFragment"
        private const val DEFAULT_WHIP_URL = "https://demo.easygbs.com:10010/live/test0508_01.whip"

        fun newInstance(): WhipFragment = WhipFragment()
    }

    private var whipModule: WhipModule? = null
    private var isRunning = false
    private var isLocalPreviewStarted = false
    private lateinit var localPreview: TextureView
    private var localSurfaceTexture: SurfaceTexture? = null

    private lateinit var etWhipUrl: EditText
    private lateinit var tvStatus: TextView
    private lateinit var btnStart: Button
    private lateinit var btnStop: Button

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_whip, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        AppLogStore.appendCritical(TAG, "onViewCreated: initialize whip UI")

        etWhipUrl = view.findViewById(R.id.et_whip_url)
        tvStatus = view.findViewById(R.id.tv_status)
        btnStart = view.findViewById(R.id.btn_start)
        btnStop = view.findViewById(R.id.btn_stop)
        localPreview = view.findViewById(R.id.local_preview_)
        localPreview.post {
            localPreview.layoutParams.height = localPreview.layoutParams.width * cameraVideoHeight /cameraVideoWidth;
        }

        etWhipUrl.setText(DEFAULT_WHIP_URL)
        tvStatus.movementMethod = ScrollingMovementMethod()
        localPreview.surfaceTextureListener = this

        btnStart.setOnClickListener { startWhipPush() }
        btnStop.setOnClickListener { stopWhipPush() }

        updateButtonState()

        EasyRTCSdk.getInstance()
        EasyRTCSdk.setEventListener(this)
        AppLogStore.appendCritical(TAG, "EasyRTCSdk listener attached for WHIP")
    }

    /** Camera video size — landscape (native sensor) dimensions. */
    private var cameraVideoWidth = 1280
    private var cameraVideoHeight = 720

    /**
     * Apply rotation + aspect-ratio-preserving scale on the TextureView.
     *
     * The preview SurfaceTexture buffer is set to landscape (e.g. 1280×720) to match
     * standard camera resolutions. The camera delivers landscape frames, so we rotate
     * 90° CW and center-crop scale to fill the portrait view.
     */
    private fun updatePreviewTransform(viewWidth: Int, viewHeight: Int) {
//        if (viewWidth == 0 || viewHeight == 0) return
//        val cx = viewWidth / 2f
//        val cy = viewHeight / 2f
//        val bufW = cameraVideoWidth.toFloat()   // 1280
//        val bufH = cameraVideoHeight.toFloat()  // 720
//
//        val matrix = Matrix()
//        // 1) Undo the default stretch (TextureView maps 1280×720 → viewW×viewH)
//        //    This restores the buffer to its native 1280×720 pixel aspect, centered.
//        matrix.setScale(bufW / viewWidth, bufH / viewHeight, cx, cy)
//        // 2) Rotate 90° CW around center → content becomes 720 wide × 1280 tall
//        matrix.postRotate(90f, cx, cy)
//        // 3) Uniformly scale to fill the view (center-crop)
//        val fillScale = maxOf(viewWidth / bufH, viewHeight / bufW)
//        matrix.postScale(fillScale, fillScale, cx, cy)
//
//        AppLogStore.appendCritical(TAG,
//            "updatePreviewTransform: view=${viewWidth}x${viewHeight} buf=${bufW.toInt()}x${bufH.toInt()} fillScale=$fillScale")
//        localPreview.setTransform(matrix)
    }

    private fun startWhipPush() {
        val url = etWhipUrl.text.toString().trim()
        if (url.isEmpty()) {
            appendLog("请输入 WHIP 地址")
            return
        }

        if (isRunning) {
            appendLog("已经在推流中")
            return
        }

        isRunning = true
        updateButtonState()
        appendLog("=== 开始推流 ===")
        appendLog("WHIP URL: $url")
        AppLogStore.appendCritical(TAG, "startWhipPush: url=$url")

        try {
            whipModule = WhipModule(url)

            EasyRTCSdk.connection(
                stun = "",
                turn = "",
                username = "",
                credential = ""
            )

            EasyRTCSdk.bindMediaSession(session)
            session.removeTransceivers()
            val encodeConfig = getVideoEncodeConfig()
            cameraVideoWidth = encodeConfig.getWidth()
            cameraVideoHeight = encodeConfig.getHeight()
            localPreview.post {
                updatePreviewTransform(localPreview.width, localPreview.height)
            }
            session.setupVideoEncoder(encodeConfig)
            session.addSendOnlyTransceivers(
                videoCodec = if (SPUtil.getInstance().getIsHevc()) EasyRTCCodec.H265 else EasyRTCCodec.H264,
                audioCodec = EasyRTCCodec.ALAW
            )
            AppLogStore.appendCritical(
                TAG,
                "WHIP transceivers ready: videoCodec=${if (SPUtil.getInstance().getIsHevc()) "H265" else "H264"} audioCodec=OPUS"
            )
            startLocalPreviewIfAvailable()
            appendLog("创建 Offer SDP...")
            EasyRTCSdk.createOffer()

        } catch (e: Exception) {
            appendLog("启动失败: ${e.message}")
            AppLogStore.appendCritical(TAG, "startWhipPush failed", e)
            isRunning = false
            updateButtonState()
        }
    }

    private fun stopWhipPush() {
        if (!isRunning) {
            appendLog("未在推流中")
            return
        }

        isRunning = false
        updateButtonState()
        appendLog("停止推流...")
        AppLogStore.appendCritical(TAG, "stopWhipPush: stopping WHIP stream")

        try {
            session.removeTransceivers()
            stopLocalPreviewIfStarted()
            EasyRTCSdk.release()
            whipModule = null
            appendLog("=== 推流已停止 ===")
        } catch (e: Exception) {
            appendLog("停止推流出错: ${e.message}")
            AppLogStore.appendCritical(TAG, "stopWhipPush failed", e)
        }
    }

    override fun connectionStateChange(state: Int) {
        session.setConnectState(state)
        when (state) {
            EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CONNECTED -> appendLog("连接成功，推流中...")
            EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_FAILED -> appendLog("连接失败")
            EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CLOSED -> appendLog("连接关闭")
        }
        AppLogStore.appendCritical(TAG, "connectionStateChange: state=$state")
    }

    override fun onSDPCallback(isOffer: Int, sdp: String) {
        if (isOffer != 1) return
        appendLog("发送 Offer SDP 到服务器...")
        whipModule?.postOffer(
            offerSdp = sdp,
            onSuccess = { answerSdp ->
                appendLog("收到 Answer SDP，长度: ${answerSdp.length}")
                AppLogStore.appendCritical(TAG, "onSDPCallback: answer received length=${answerSdp.length}")
                runOnMainThread {
                    try {
                        EasyRTCSdk.setRemoteDescription(answerSdp)
                        appendLog("设置远端描述完成")
                        AppLogStore.appendCritical(TAG, "setRemoteDescription success")
                    } catch (e: Exception) {
                        appendLog("设置远端描述失败: ${e.message}")
                        AppLogStore.appendCritical(TAG, "setRemoteDescription failed", e)
                    }
                }
            },
            onError = { error ->
                appendLog(error)
                AppLogStore.appendCritical(TAG, "postOffer failed: $error")
            }
        )
    }

    override fun onTransceiverCallback(
        track: Int,
        codecId: Int,
        frameType: Int,
        frameData: ByteArray,
        frameSize: Int,
        pts: Long,
    ) {
        // WHIP 推流仅发送本地媒体，不处理远端音视频帧
    }

    override fun onDataChannelCallback(type: Int, binary: Int, data: ByteArray, size: Int) {
    }

    override fun onRemoteVideoSize(width: Int, height: Int) {
        // WHIP 推流仅发送，不接收远端视频
    }

    override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
        if (surface == localPreview.surfaceTexture) {
            val config = getVideoEncodeConfig()
            surface.setDefaultBufferSize(config.getWidth(), config.getHeight())  // 1280×720 landscape
            AppLogStore.appendCritical(TAG,"主视频 SurfaceTexture 已创建, buffer=${config.getWidth()}x${config.getHeight()}")
            localSurfaceTexture = surface
            updatePreviewTransform(width, height)
            startLocalPreviewIfAvailable()
        }
    }

    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {
        if (surface == localPreview.surfaceTexture) {
            updatePreviewTransform(width, height)
        }
    }

    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
        if (surface == localPreview.surfaceTexture) {
            localSurfaceTexture = null
            stopLocalPreviewIfStarted()
        }
        return true
    }

    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {
    }

    private fun startLocalPreviewIfAvailable() {
        if (!hasCameraPermission()) {
            appendLog("无相机权限，无法启动预览")
            AppLogStore.appendCritical(TAG, "startLocalPreviewIfAvailable: camera permission denied")
            return
        }
        if (localSurfaceTexture == null || !localPreview.isAvailable) return
        if (isLocalPreviewStarted) {
            AppLogStore.appendCritical(TAG, "startLocalPreviewIfAvailable skipped: preview already running")
            return
        }
        val result = session.startPreview(Surface(localSurfaceTexture))
        if (result != 0) {
            appendLog("本地预览启动失败: $result")
            Log.e(TAG, "startPreview failed: $result")
            AppLogStore.appendCritical(TAG, "startPreview failed: ret=$result")
        } else {
            isLocalPreviewStarted = true
            appendLog("本地预览已启动")
            AppLogStore.appendCritical(TAG, "startPreview success")
        }
    }

    private fun stopLocalPreviewIfStarted() {
        if (!isLocalPreviewStarted) return
        session.stopPreview()
        isLocalPreviewStarted = false
        AppLogStore.appendCritical(TAG, "stopPreview success")
    }

    private fun appendLog(message: String) {
        AppLogStore.appendTimestamped("[WHIP][$TAG] $message")
        runOnMainThread {
            val currentText = tvStatus.text.toString()
            val newText = if (currentText.isEmpty()) message else "$currentText\n$message"
            tvStatus.text = newText
            tvStatus.post {
                tvStatus.layout(
                    tvStatus.left, tvStatus.top,
                    tvStatus.right, tvStatus.bottom
                )
                val amount = tvStatus.layout?.height ?: 0
                if (amount > 0) {
                    tvStatus.scrollTo(0, tvStatus.height)
                }
            }
        }
    }

    private fun updateButtonState() {
        btnStart.isEnabled = !isRunning
        btnStop.isEnabled = isRunning
    }

    override fun onDestroyView() {
        try {
            AppLogStore.appendCritical(TAG, "onDestroyView: releasing WHIP resources, isRunning=$isRunning")
            if (isRunning) {
                session.removeTransceivers()
                stopLocalPreviewIfStarted()
            }
            EasyRTCSdk.unsetEventListener(this)
            EasyRTCSdk.release()
        } catch (e: Exception) {
            Log.e(TAG, "release failed", e)
            AppLogStore.appendCritical(TAG, "onDestroyView release failed", e)
        }
        super.onDestroyView()
    }

    override fun onDestroy() {
        whipModule = null
        super.onDestroy()
    }
}
