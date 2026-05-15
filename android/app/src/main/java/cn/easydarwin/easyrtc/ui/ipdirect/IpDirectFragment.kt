package cn.easydarwin.easyrtc.ui.ipdirect

import android.content.Context
import android.graphics.SurfaceTexture
import android.media.AudioManager
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.Surface
import android.view.TextureView
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.TextView
import androidx.appcompat.widget.PopupMenu
import cn.easydarwin.easyrtc.MainActivity
import cn.easydarwin.easyrtc.R
import cn.easyrtc.EasyRTCCodec
import cn.easyrtc.EasyRTCIceTransportPolicy
import cn.easyrtc.EasyRTCPeerConnectionState
import cn.easyrtc.EasyRTCSdk
import cn.easydarwin.easyrtc.ui.live.BaseRtcMediaFragment
import cn.easydarwin.easyrtc.ui.live.LiveSessionController
import cn.easydarwin.easyrtc.ui.live.LiveUiState
import cn.easydarwin.easyrtc.ui.live.NativePipelineController
import cn.easydarwin.easyrtc.ui.live.NativePipelineState
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil
import java.net.Inet4Address
import java.net.NetworkInterface
import java.util.Locale

/**
 * IP直连 fragment — acts as a WebSocket server on port 9000.
 * Accepts incoming calls from WS clients and handles bidirectional media.
 * No device list, no outgoing calls — passive answer only.
 */
class IpDirectFragment : BaseRtcMediaFragment(), TextureView.SurfaceTextureListener,
    EasyRTCSdk.EasyRTCEventListener, IpDirectServer.Listener {

    companion object {
        private const val TAG = "IpDirectFragment"
    }

    // UI
    private lateinit var endCallButton: ImageButton
    private lateinit var switchCameraButton: ImageButton
    private lateinit var speakerButton: ImageButton
    private lateinit var localPreview: TextureView
    private lateinit var remotePreview: TextureView
    private lateinit var remotePreviewContainer: View
    private lateinit var tvStatus: TextView
    private var bandwidthTV: TextView? = null
    private var buttonMenu: ImageButton? = null

    private val audioManager by lazy {
        requireContext().getSystemService(Context.AUDIO_SERVICE) as AudioManager
    }

    // Surface state
    private var mainSurfaceTexture: SurfaceTexture? = null
    private var smallSurfaceTexture: SurfaceTexture? = null

    // Pipeline & session state
    private val pipelineController = NativePipelineController()
    private val liveSessionController = LiveSessionController()

    // WebSocket server
    private var server: IpDirectServer? = null

    override fun onMediaSessionCreated(session: cn.easyrtc.media.MediaSession) {
        session.setInputKbpsStatsListener { stats ->
            runOnMainThread {
                bandwidthTV?.text = String.format(
                    Locale.getDefault(),
                    "kbps: v:%.2f a:%.2f",
                    stats.videoKbps,
                    stats.audioKbps
                )
            }
        }
    }

    override fun onMediaSessionReleasing(session: cn.easyrtc.media.MediaSession) {
        session.setInputKbpsStatsListener(null)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_ip_direct, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        EasyRTCSdk.getInstance()
        EasyRTCSdk.setEventListener(this)

        initViews(view)
        observeLiveSessionState()
        startServer()
    }

    private fun initViews(view: View) {
        localPreview = view.findViewById(R.id.local_preview_)
        localPreview.post {
            localPreview.layoutParams.height =
                localPreview.layoutParams.width * getVideoEncodeConfig().getHeight() / getVideoEncodeConfig().getWidth()
        }
        remotePreview = view.findViewById(R.id.remote_preview_)
        remotePreviewContainer = view.findViewById(R.id.remote_preview_container)
        tvStatus = view.findViewById(R.id.tvIpDirectStatus)
        endCallButton = view.findViewById(R.id.endCallButton)
        switchCameraButton = view.findViewById(R.id.switchCameraButton)
        speakerButton = view.findViewById(R.id.speakerButton)
        bandwidthTV = view.findViewById(R.id.media_bandwidth)
        buttonMenu = view.findViewById(R.id.button_ip_direct_menu)

        endCallButton.visibility = View.GONE

        localPreview.surfaceTextureListener = this
        remotePreview.surfaceTextureListener = this

        endCallButton.setOnClickListener {
            Log.d(TAG, "挂断通话")
            server?.sendHangup()
            stopCall()
        }

        switchCameraButton.setOnClickListener { switchCamera() }

        speakerButton.setOnClickListener {
            audioManager.isSpeakerphoneOn = !audioManager.isSpeakerphoneOn
            val on = audioManager.isSpeakerphoneOn
            speakerButton.setImageResource(if (on) R.drawable.ic_speaker_on else R.drawable.ic_speaker_off)
            appendLog(if (on) "扬声器已开启" else "扬声器已关闭")
        }

        buttonMenu?.setOnClickListener { showMenu(it) }
    }

    // ─── Server lifecycle ────────────────────────────────────────────────

    private fun startServer() {
        if (server?.isRunning == true) return
        server = IpDirectServer(IpDirectServer.DEFAULT_PORT, this)
        server?.start()
    }

    private fun stopServer() {
        server?.stop()
        server = null
    }

    // ─── IpDirectServer.Listener ─────────────────────────────────────────

    override fun onServerStarted(port: Int) {
        val ip = getLocalIpAddress() ?: "unknown"
        tvStatus.text = "监听中 ws://$ip:$port"
        appendLog("服务已启动 ws://$ip:$port")
    }

    override fun onServerStopped() {
        appendLog("服务已停止")
    }

    override fun onServerError(message: String) {
        appendLog("服务错误: $message")
    }

    override fun onClientConnected(remoteAddress: String) {
        appendLog("客户端连接: $remoteAddress")
        tvStatus.text = "客户端已连接: $remoteAddress"
    }

    override fun onClientDisconnected(remoteAddress: String) {
        appendLog("客户端断开: $remoteAddress")
        stopCall()
        val ip = getLocalIpAddress() ?: "unknown"
        tvStatus.text = "监听中 ws://$ip:${IpDirectServer.DEFAULT_PORT}"
    }

    override fun onOfferReceived(sdp: String) {
        appendLog("收到 Offer SDP, 长度=${sdp.length}")
        handleIncomingOffer(sdp)
    }

    override fun onHangup() {
        appendLog("对方挂断")
        stopCall()
    }

    // ─── Call handling (passive answer) ──────────────────────────────────

    private fun handleIncomingOffer(offerSdp: String) {
        // IP直连 — no STUN/TURN needed, direct LAN connection
        EasyRTCSdk.connection(
            "",  // stun
            "",  // turn
            "",  // username
            "",  // credential
            1,
            EasyRTCIceTransportPolicy.EaSyRTC_ICE_TRANSPORT_POLICY_ALL,
            0L
        )

        // Parse codecs from offer SDP
        var videoCodec = 0
        var audioCodec = 0

        if (offerSdp.contains("m=video", ignoreCase = true)) {
            if (offerSdp.contains("H264/90000")) videoCodec = EasyRTCCodec.H264
            else if (offerSdp.contains("H265/90000")) videoCodec = EasyRTCCodec.H265
            else if (offerSdp.contains("VP8/90000")) videoCodec = EasyRTCCodec.VP8
        }

        if (offerSdp.contains("m=audio", ignoreCase = true)) {
            if (offerSdp.contains("PCMA/8000", ignoreCase = true)) audioCodec = EasyRTCCodec.ALAW
            else if (offerSdp.contains("PCMU/8000", ignoreCase = true)) audioCodec = EasyRTCCodec.MULAW
            else if (offerSdp.contains("opus/48000", ignoreCase = true)) audioCodec = EasyRTCCodec.OPUS
        }

        if (videoCodec == 0 && audioCodec == 0) {
            appendLog("Offer SDP 中未发现可用编码")
            return
        }

        EasyRTCSdk.bindMediaSession(session)
        session.removeTransceivers()
        session.addTransceivers(videoCodec, audioCodec)

        if (offerSdp.contains("webrtc-datachannel", ignoreCase = true)) {
            EasyRTCSdk.addDataChannel()
        }

        appendLog("创建 Answer...")
        EasyRTCSdk.createAnswer(offerSdp)
    }

    private fun stopCall() {
        session.removeTransceivers()
        pipelineController.stop()
        liveSessionController.onClosed()
    }

    // ─── EasyRTCSdk.EasyRTCEventListener ─────────────────────────────────

    override fun connectionStateChange(state: Int) {
        Log.d(TAG, "connectionStateChange state=$state")
        session.setConnectState(state)
        when (state) {
            EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CONNECTED -> {
                activity?.runOnUiThread {
                    session.requestKeyFrame()
                    liveSessionController.onConnected("IP直连")
                }
            }
            EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_FAILED -> {
                liveSessionController.onFailed()
            }
            EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CLOSED -> {
                liveSessionController.onClosed()
            }
        }
    }

    override fun onSDPCallback(isOffer: Int, sdp: String) {
        if (isOffer == 0) {
            // Answer SDP generated — send to client via WebSocket
            appendLog("Answer SDP 已生成, 发送到客户端")
            server?.sendAnswer(sdp)
        }
    }

    override fun onTransceiverCallback(
        track: Int, codecId: Int, frameType: Int,
        frameData: ByteArray, frameSize: Int, pts: Long
    ) {}

    override fun onRemoteVideoSize(width: Int, height: Int) {
        activity?.runOnUiThread {
            val density = resources.displayMetrics.density

            val desiredWidthPx = remotePreviewContainer.width
            val desiredHeightPx = desiredWidthPx * height / width
            val lp = remotePreviewContainer.layoutParams
            lp.width = desiredWidthPx
            lp.height = desiredHeightPx
            remotePreviewContainer.layoutParams = lp

            val desiredWidthDp = 120
            localPreview.layoutParams.width = Math.round(desiredWidthDp * density)
            localPreview.layoutParams.height = Math.round(localPreview.layoutParams.width * 1280f / 720f)
            localPreview.requestLayout()
            localPreview.setOnTouchListener { view, event ->
                when (event.action) {
                    MotionEvent.ACTION_DOWN -> {
                        view.tag = floatArrayOf(event.rawX - view.translationX, event.rawY - view.translationY)
                        true
                    }
                    MotionEvent.ACTION_MOVE -> {
                        val anchor = view.tag as? FloatArray ?: return@setOnTouchListener false
                        view.translationX = event.rawX - anchor[0]
                        view.translationY = event.rawY - anchor[1]
                        true
                    }
                    else -> false
                }
            }
        }
    }

    override fun onDataChannelCallback(type: Int, binary: Int, data: ByteArray, size: Int) {
        if (type == 1) {
            EasyRTCSdk.sendMsg(0, "Hello EasyRTC IP直连!!!".toByteArray(Charsets.UTF_8))
        } else if (type == 2 && binary == 0) {
            requireActivity().runOnUiThread {
                appendLog(data.toString(Charsets.UTF_8))
            }
        }
    }

    // ─── LiveSession state observation ───────────────────────────────────

    private fun observeLiveSessionState() {
        liveSessionController.state.observe(viewLifecycleOwner) { state ->
            when (state) {
                is LiveUiState.Idle -> {
                    requireActivity().runOnUiThread {
                        val ip = getLocalIpAddress() ?: "unknown"
                        tvStatus.text = "监听中 ws://$ip:${IpDirectServer.DEFAULT_PORT}"
                        endCallButton.visibility = View.GONE
                    }
                }
                is LiveUiState.Connected -> {
                    appendLog("连接成功")
                    requireActivity().runOnUiThread {
                        endCallButton.visibility = View.VISIBLE
                        tvStatus.text = "IP直连 [已连接]"
                    }
                }
                is LiveUiState.Disconnected -> {
                    appendLog("连接断开")
                    requireActivity().runOnUiThread {
                        val ip = getLocalIpAddress() ?: "unknown"
                        tvStatus.text = "监听中 ws://$ip:${IpDirectServer.DEFAULT_PORT} [已断开]"
                        endCallButton.visibility = View.INVISIBLE
                    }
                }
                is LiveUiState.Failed -> {
                    appendLog("连接失败")
                    state.reason?.takeIf { it.isNotBlank() }?.let { appendLog("原因: $it") }
                    requireActivity().runOnUiThread {
                        tvStatus.text = "IP直连 [连接失败]"
                        endCallButton.visibility = View.INVISIBLE
                    }
                }
            }
        }
    }

    // ─── TextureView.SurfaceTextureListener ──────────────────────────────

    override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
        when (surface) {
            localPreview.surfaceTexture -> {
                mainSurfaceTexture = surface
                val config = getVideoEncodeConfig()
                surface.setDefaultBufferSize(config.getWidth(), config.getHeight())
                Log.d(TAG, "本地 SurfaceTexture 已创建, buffer=${config.getWidth()}x${config.getHeight()}")
            }
            remotePreview.surfaceTexture -> {
                smallSurfaceTexture = surface
                session.setupVideoEncoder(getVideoEncodeConfig())
                session.setDecoderSurface(Surface(surface))
                Log.d(TAG, "远端 SurfaceTexture 已创建")
            }
        }
        if (mainSurfaceTexture != null && smallSurfaceTexture != null && hasCameraPermission()) {
            startCameraPreview()
        }
    }

    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {}

    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
        when {
            surface == localPreview.surfaceTexture -> mainSurfaceTexture = null
            surface == remotePreview.surfaceTexture -> {
                smallSurfaceTexture = null
                session.setDecoderSurface(null)
            }
        }
        return true
    }

    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {}

    // ─── Camera ──────────────────────────────────────────────────────────

    private fun startCameraPreview() {
        if (!hasCameraPermission()) return
        try {
            if (localPreview.isAvailable) {
                val result = session.startPreview(Surface(localPreview.surfaceTexture))
                if (result != 0) {
                    Log.e(TAG, "startPreview failed: $result")
                    pipelineController.reportError("startPreview failed: $result")
                    return
                }
            }
            pipelineController.start()
        } catch (e: Exception) {
            Log.e(TAG, "Camera preview failed: ${e.message}")
            pipelineController.reportError(e.message ?: "unknown")
        }
    }

    private fun switchCamera() {
        if (pipelineController.state !is NativePipelineState.Running) return
        session.switchCamera()
        pipelineController.switchCamera()
    }

    // ─── Lifecycle ───────────────────────────────────────────────────────

    override fun onResume() {
        super.onResume()
        if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
            startCameraPreview()
        }
    }

    override fun onPause() {
        super.onPause()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        stopCall()
        session.stopPreview()
        EasyRTCSdk.unsetEventListener(this)
        EasyRTCSdk.release()
        stopServer()
        buttonMenu = null
    }

    // ─── Menu ────────────────────────────────────────────────────────────

    private fun showMenu(anchor: View) {
        PopupMenu(requireContext(), anchor).apply {
            menu.add(0, 1, 0, "设置")
            setOnMenuItemClickListener { item ->
                when (item.itemId) {
                    1 -> {
                        (activity as? MainActivity)?.openSettingsScreen()
                        true
                    }
                    else -> false
                }
            }
            show()
        }
    }

    // ─── Helpers ─────────────────────────────────────────────────────────

    private fun appendLog(message: String) {
        AppLogStore.appendTimestamped("[IP直连] $message")
    }

    fun onPermissionGranted() {
        if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
            startCameraPreview()
        }
    }

    private fun getLocalIpAddress(): String? {
        try {
            val interfaces = NetworkInterface.getNetworkInterfaces()
            while (interfaces.hasMoreElements()) {
                val intf = interfaces.nextElement()
                if (intf.isLoopback || !intf.isUp) continue
                val addresses = intf.inetAddresses
                while (addresses.hasMoreElements()) {
                    val addr = addresses.nextElement()
                    if (addr is Inet4Address && !addr.isLoopbackAddress) {
                        return addr.hostAddress
                    }
                }
            }
        } catch (_: Exception) {}
        return null
    }
}
