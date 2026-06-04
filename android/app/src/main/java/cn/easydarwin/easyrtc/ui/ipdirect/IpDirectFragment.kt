package cn.easydarwin.easyrtc.ui.ipdirect

import android.content.Context
import android.graphics.SurfaceTexture
import android.hardware.Camera
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
import cn.easydarwin.easyrtc.ui.live.BaseRtcMediaFragment
import cn.easyrtc.model.LiveUiState
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easyrtc.model.DataChannelEvent
import java.net.Inet4Address
import java.net.NetworkInterface
import java.util.Locale

class IpDirectFragment : BaseRtcMediaFragment(), TextureView.SurfaceTextureListener, IpDirectServer.Listener {

    companion object {
        private const val TAG = "IpDirectFragment"
    }

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

    private var mainSurfaceTexture: SurfaceTexture? = null
    private var smallSurfaceTexture: SurfaceTexture? = null

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
        initViews(view)
        setupSessionObservers()
        startServer()
    }

    private fun setupSessionObservers() {
        val s = session ?: return
        s.connectionState.observe(viewLifecycleOwner) { state ->
            when (state) {
                is LiveUiState.Idle -> {
                    val ip = getLocalIpAddress() ?: "unknown"
                    tvStatus.text = "监听中 ws://$ip:${IpDirectServer.DEFAULT_PORT}"
                    endCallButton.visibility = View.GONE
                }
                is LiveUiState.Connected -> {
                    appendLog("连接成功")
                    endCallButton.visibility = View.VISIBLE
                    tvStatus.text = "IP直连 [已连接]"
                }
                is LiveUiState.Disconnected -> {
                    appendLog("连接断开")
                    resetCallUI()
                }
                is LiveUiState.Failed -> {
                    appendLog("连接失败")
                    state.reason?.takeIf { it.isNotBlank() }?.let { appendLog("原因: $it") }
                    resetCallUI()
                }
            }
        }

        s.dataChannel.observe(viewLifecycleOwner) { event ->
            if (event is DataChannelEvent.Open) {
                s.sendDataChannelMsg(false, "Hello EasyRTC IP直连!!!".toByteArray(Charsets.UTF_8))
            } else if (event is DataChannelEvent.Message) {
                if (event.binary == 0) {
                    runOnMainThread { appendLog(event.data.toString(Charsets.UTF_8)) }
                }
            }
        }

        s.remoteVideoSize.observe(viewLifecycleOwner) { size ->
            if (size.width > 0 && size.height > 0) onRemoteVideoSize(size.width, size.height)
        }
    }

    private fun initViews(view: View) {
        localPreview = view.findViewById(R.id.local_preview_)
        resetLocalPreview(true)
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
            server?.sendHangup()
            resetCallUI()
            session.releasePeerConnection()
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

    // ─── Server ───────────────────────────────────────────────────────────

    private fun startServer() {
        if (server?.isRunning == true) return
        server = IpDirectServer(IpDirectServer.DEFAULT_PORT, this)
        server?.start()
    }

    private fun stopServer() {
        server?.stop()
        server = null
    }

    override fun onServerStarted(port: Int) {
        val ip = getLocalIpAddress() ?: "unknown"
        tvStatus.text = "监听中 ws://$ip:$port"
        appendLog("服务已启动 ws://$ip:$port")
    }
    override fun onServerStopped() { appendLog("服务已停止") }
    override fun onServerError(message: String) { appendLog("服务错误: $message") }
    override fun onClientConnected(remoteAddress: String) {
        appendLog("客户端连接: $remoteAddress")
        tvStatus.text = "客户端已连接: $remoteAddress"
    }
    override fun onClientDisconnected(remoteAddress: String) {
        appendLog("客户端断开: $remoteAddress")
        resetCallUI()
        session.releasePeerConnection()
    }
    override fun onClientReady() {
        appendLog("客户端握手完成，创建 Offer...")
        createAndSendOffer()
    }
    override fun onAnswerReceived(sdp: String) {
        appendLog("收到 Answer SDP, 长度=${sdp.length}")
        session.setRemoteDescription(sdp)
    }

    private fun resetCallUI() {
        val ip = getLocalIpAddress() ?: "unknown"
        tvStatus.text = "监听中 ws://$ip:${IpDirectServer.DEFAULT_PORT}"
        endCallButton.visibility = View.GONE
        remotePreviewContainer.visibility = View.GONE
        resetLocalPreview(true)
        localPreview.translationX = 0f
        localPreview.translationY = 0f
        bandwidthTV?.text = ""
    }

    private fun createAndSendOffer() {
        session.releasePeerConnection()
        session.createPeerConnection("", "", "", "")
        val config = getVideoEncodeConfig()
        val videoCodec = if (config.getUseHevc()) EasyRTCCodec.H265 else EasyRTCCodec.H264
        session.addTransceivers(videoCodec, EasyRTCCodec.ALAW)
        session.startSend()
        session.addDataChannel("")
        appendLog("创建 Offer...")
        session.createOffer { sdp ->
            appendLog("Offer 创建完成, 长度=${sdp.length}")
            server?.sendOffer(sdp)
        }
    }

    fun onRemoteVideoSize(width: Int, height: Int) {
        val density = resources.displayMetrics.density
        val desiredWidthPx = remotePreviewContainer.width
        val desiredHeightPx = desiredWidthPx * height / width
        val lp = remotePreviewContainer.layoutParams
        lp.width = desiredWidthPx
        lp.height = desiredHeightPx
        remotePreviewContainer.layoutParams = lp

        resetLocalPreview(false)
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

    fun onDataChannelCallback(type: Int, binary: Int, data: ByteArray, size: Int) {
        if (type == 1) {
            session.sendDataChannelMsg(false, "Hello EasyRTC IP直连!!!".toByteArray(Charsets.UTF_8))
        } else if (type == 2 && binary == 0) {
            requireActivity().runOnUiThread { appendLog(data.toString(Charsets.UTF_8)) }
        }
    }

    // ─── SurfaceTexture lifecycle ─── drives session create / release ────

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
                Log.d(TAG, "远端 SurfaceTexture 已创建")
            }
        }
        if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
            session.setupVideoEncoder(getVideoEncodeConfig())
            session.setDecoderSurface(Surface(remotePreview.surfaceTexture))
            startCameraPreview()
        }
    }

    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {}

    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
        when {
            surface == localPreview.surfaceTexture -> mainSurfaceTexture = null
            surface == remotePreview.surfaceTexture -> smallSurfaceTexture = null
        }
        if (mainSurfaceTexture == null && smallSurfaceTexture == null) {
            session.releasePeerConnection()
            session.stopPreview()
            session.setDecoderSurface(null)
        }
        return true
    }

    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {}

    // ─── Camera ──────────────────────────────────────────────────────────

    private fun startCameraPreview() {
        try {
            if (localPreview.isAvailable) {
                val result = session.startPreview(Surface(localPreview.surfaceTexture)) ?: -1
                if (result != 0) {
                    Log.e(TAG, "startPreview failed: $result")
                    return
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Camera preview failed: ${e.message}")
        }
    }

    private fun switchCamera() {
        val config = getVideoEncodeConfig()
        mainSurfaceTexture?.setDefaultBufferSize(config.getWidth(), config.getHeight())
        session.switchCamera()
    }

    // ─── Lifecycle ───────────────────────────────────────────────────────

    override fun onDestroyView() {
        super.onDestroyView()
        stopServer()
        buttonMenu = null
    }

    private fun showMenu(anchor: View) {
        PopupMenu(requireContext(), anchor).apply {
            menu.add(0, 1, 0, "设置")
            setOnMenuItemClickListener { item ->
                when (item.itemId) {
                    1 -> { (activity as? MainActivity)?.openSettingsScreen(); true }
                    else -> false
                }
            }
            show()
        }
    }

    private fun appendLog(message: String) { AppLogStore.appendTimestamped("[IP直连] $message") }

    private fun getLocalIpAddress(): String? {
        try {
            val interfaces = NetworkInterface.getNetworkInterfaces()
            while (interfaces.hasMoreElements()) {
                val intf = interfaces.nextElement()
                if (intf.isLoopback || !intf.isUp) continue
                val addresses = intf.inetAddresses
                while (addresses.hasMoreElements()) {
                    val addr = addresses.nextElement()
                    if (addr is Inet4Address && !addr.isLoopbackAddress) return addr.hostAddress
                }
            }
        } catch (_: Exception) {}
        return null
    }
}
