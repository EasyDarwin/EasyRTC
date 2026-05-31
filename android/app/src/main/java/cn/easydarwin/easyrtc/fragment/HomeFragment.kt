package cn.easydarwin.easyrtc.fragment

import android.content.Context
import android.graphics.SurfaceTexture
import android.media.AudioManager
import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.util.Log
import android.view.MotionEvent
import android.view.LayoutInflater
import android.view.Surface
import android.view.TextureView
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.widget.PopupMenu
import cn.easydarwin.easyrtc.MainActivity
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.service.WebSocketService
import cn.easyrtc.EasyRTCUser
import cn.easyrtc.EasyRTCPeerConnectionState
import cn.easyrtc.EasyRTCStreamTrack
import cn.easyrtc.helper.MagicFileHelper
import cn.easyrtc.media.MediaSession
import cn.easydarwin.easyrtc.ui.live.BaseRtcMediaFragment
import cn.easydarwin.easyrtc.ui.hub.HubFragment

import cn.easydarwin.easyrtc.BuildConfig
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easydarwin.easyrtc.utils.WebSocketManager
import cn.easyrtc.EasyRTCCodec
import cn.easyrtc.model.DataChannelEvent
import cn.easyrtc.model.LiveUiState
import org.json.JSONObject
import java.util.Locale

class HomeFragment : BaseRtcMediaFragment(), TextureView.SurfaceTextureListener {

    companion object {
        private const val TAG = "HomeFragment"
    }

    private lateinit var endCallButton: ImageButton
    private lateinit var switchCameraButton: ImageButton
    private lateinit var speakerButton: ImageButton
    private val audioManager by lazy { requireContext().getSystemService(Context.AUDIO_SERVICE) as AudioManager }

    private lateinit var local_preview_: TextureView
    private lateinit var tvFragmentUUID: TextView
    private lateinit var remote_preview_: TextureView
    private lateinit var remote_preview_container: View

    private var mainSurfaceTexture: SurfaceTexture? = null
    private var smallSurfaceTexture: SurfaceTexture? = null

    private lateinit var mMagicFileHelper: MagicFileHelper

    private var webSocketService: WebSocketService? = null

    private var buttonHomeMenu: ImageButton? = null
    private var userList: List<EasyRTCUser> = emptyList()

    private var activeSessionUser: String? = null

    private var bandwidthTV: TextView? = null

    override fun onMediaSessionCreated(session: MediaSession) {
        session.setInputKbpsStatsListener { stats ->
            runOnMainThread {
                bandwidthTV?.text = String.format(
                    Locale.getDefault(),
                    "kbps: v:%.2f a:%.2f",
                    stats.videoKbps,
                    stats.audioKbps,
                    stats.totalKbps
                )
            }
        }
    }

    override fun onMediaSessionReleasing(session: MediaSession) {
        session.setInputKbpsStatsListener(null)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        mMagicFileHelper = MagicFileHelper.getInstance()
        return inflater.inflate(R.layout.fragment_home, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        initViews(view)
        setupSessionObservers()

        (activity as? MainActivity)?.webSocketServiceLiveData?.observe(viewLifecycleOwner) { service ->
            webSocketService = service
            service?.events?.observe(viewLifecycleOwner) { event ->
                when (event) {
                    is WebSocketService.Event.Connected -> onWSConnected()
                    is WebSocketService.Event.Message -> onWSMessage(event.text)
                    is WebSocketService.Event.Disconnected -> onWSDisconnected(event.code, event.reason)
                    is WebSocketService.Event.Error -> onWSError(event.throwable)
                    is WebSocketService.Event.Logs -> onWSLogs(event.text)
                    is WebSocketService.Event.IncomingCall -> {}
                    is WebSocketService.Event.AnswerSDP -> {
                        if (event.handled) return@observe
                        event.handled = true
                        session.setRemoteDescription(event.sdp)
                    }
                    is WebSocketService.Event.OnlineUsers -> updateOnlineUsers(event.users)
                }
            }
        }

        (activity as? MainActivity)?.incomingCallLiveData?.observe(viewLifecycleOwner) { event ->
            if (event.handled) return@observe
            event.handled = true
            activeSessionUser = event.uuid
            tvFragmentUUID.text = "来电: ${event.uuid}"
            appendLog("来电: ${event.uuid}")
            view.post {
                val ws = webSocketService
                if (BuildConfig.DEBUG) check(ws != null) { "webSocketService is null when handling incoming call" }
                if (ws == null) return@post
                val callSetup = ws.prepareIncomingCall(event.data, event.callout)
                session.createPeerConnection(
                    "stun:${callSetup.stunTurnInfo.stunServer}",
                    "turn:${callSetup.stunTurnInfo.turnServer}",
                    callSetup.stunTurnInfo.turnUsername,
                    callSetup.stunTurnInfo.turnPassword)

                val config = getVideoEncodeConfig()
                val videoCodec = if (config.getUseHevc()) EasyRTCCodec.H265 else EasyRTCCodec.H264
                session.addTransceivers(videoCodec, EasyRTCCodec.ALAW)
                session.startSend()
                session.addDataChannel("")

                if (callSetup.callout) {
                    session.createAnswer(callSetup.sdp) { ws.sendOfferSDP(it, false) }
                } else {
                    session.createOffer { ws.sendOfferSDP(it, true) }
                }
            }
        }
    }

    private fun setupSessionObservers() {
        val s = session ?: return
        s.connectionState.observe(viewLifecycleOwner) { state ->
            when (state) {
                is LiveUiState.Idle -> {
                    activeSessionUser = null
                    resetCallUI()
                    tvFragmentUUID.text = SPUtil.getInstance().rtcUserUUID
                    endCallButton.visibility = View.GONE
                }
                is LiveUiState.Connected -> {
                    appendLog2("------------------------------")
                    appendLog2("------------------------------")
                    appendLog("连接成功")
                    appendLog(getIceCandidateTypeDesc())
                    endCallButton.visibility = View.VISIBLE
                    tvFragmentUUID.text = "${SPUtil.getInstance().rtcUserUUID} [已连接]"
                }
                is LiveUiState.Disconnected -> {
                    appendLog("连接断开")
                    session.releasePeerConnection()
                }
                is LiveUiState.Failed -> {
                    appendLog("连接失败")
                    state.reason?.takeIf { it.isNotBlank() }?.let { appendLog("失败原因: $it") }
                    session.releasePeerConnection()
                }
            }
        }

        s.dataChannel.observe(viewLifecycleOwner) { event ->
            when (event) {
                is DataChannelEvent.Open -> {
                    s.sendDataChannelMsg(false, "Hello EasyRTC!!!".toByteArray(Charsets.UTF_8))
                }
                is DataChannelEvent.Message -> {
                    if (event.binary == 0) appendLog(event.data.toString(Charsets.UTF_8))
                }
                else -> {}
            }
        }

        s.remoteVideoSize.observe(viewLifecycleOwner) { size ->
            if (size.width == 0 && size.height == 0) return@observe
            val density = resources.displayMetrics.density
            val desiredWidthPx = remote_preview_container.width
            val desiredHeightPx = desiredWidthPx * size.height / size.width
            val lp = remote_preview_container.layoutParams
            lp.width = desiredWidthPx
            lp.height = desiredHeightPx
            remote_preview_container.layoutParams = lp
            Log.d(TAG, "Remote video size: ${size.width}x${size.height}, container: ${lp.width}x${lp.height}")

            val desiredWidthDp = 120
            local_preview_.layoutParams.width = Math.round(desiredWidthDp * density)
            local_preview_.layoutParams.height = Math.round(local_preview_.layoutParams.width * 1280f / 720f)
            local_preview_.requestLayout()
            local_preview_.setOnTouchListener { view, event ->
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

    private fun initViews(view: View) {
        local_preview_ = view.findViewById(R.id.local_preview_)
        tvFragmentUUID = view.findViewById(R.id.tvFragmentUUID)
        remote_preview_ = view.findViewById(R.id.remote_preview_)
        remote_preview_container = view.findViewById(R.id.remote_preview_container)
        endCallButton = view.findViewById(R.id.endCallButton)
        switchCameraButton = view.findViewById(R.id.switchCameraButton)
        speakerButton = view.findViewById(R.id.speakerButton)
        bandwidthTV = view.findViewById(R.id.media_bandwidth)
        buttonHomeMenu = view.findViewById(R.id.button_home_menu)

        endCallButton.visibility = View.GONE

        tvFragmentUUID.text = SPUtil.getInstance().rtcUserUUID

        buttonHomeMenu?.setOnClickListener {
            showHomeMenu(it)
        }

        local_preview_.surfaceTextureListener = this
        remote_preview_.surfaceTextureListener = this

        endCallButton.setOnClickListener {
            Log.d(TAG, "挂电话")
            session.releasePeerConnection()
        }

        switchCameraButton.setOnClickListener {
            switchCamera()
        }

        speakerButton.setOnClickListener {
            audioManager.isSpeakerphoneOn = !audioManager.isSpeakerphoneOn
            val isSpeakerOn = audioManager.isSpeakerphoneOn
            speakerButton.setImageResource(if (isSpeakerOn) R.drawable.ic_speaker_on else R.drawable.ic_speaker_off)
            appendLog(if (isSpeakerOn) "扬声器已开启" else "扬声器已关闭")
        }

        local_preview_.post {
            val config = getVideoEncodeConfig()
            local_preview_.layoutParams.height = local_preview_.width *  config.getWidth() / config.getHeight()
            local_preview_.requestLayout()
        }
        Log.d(TAG, "视图初始化完成")
    }

    // ─── SurfaceTexture lifecycle ─── drives session create / release ────

    override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
        when (surface) {
            local_preview_.surfaceTexture -> {
                mainSurfaceTexture = surface
                val config = getVideoEncodeConfig()
                surface.setDefaultBufferSize(config.getWidth(), config.getHeight())
                Log.d(TAG, "主视频 SurfaceTexture 已创建, buffer=${config.getWidth()}x${config.getHeight()}")
            }
            remote_preview_.surfaceTexture -> {
                smallSurfaceTexture = surface
                Log.d(TAG, "小窗口 SurfaceTexture 已创建")
            }
        }

        if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
            session.setupVideoEncoder(getVideoEncodeConfig())
            session.setDecoderSurface(Surface(remote_preview_.surfaceTexture))
            startNativeCameraPreview()
        }
    }

    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {
        Log.d(TAG, "SurfaceTexture 尺寸改变: $width x $height")
    }

    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
        when {
            surface == local_preview_.surfaceTexture -> mainSurfaceTexture = null
            surface == remote_preview_.surfaceTexture -> smallSurfaceTexture = null
        }
        if (mainSurfaceTexture == null && smallSurfaceTexture == null) {
            session.releasePeerConnection()
            session.stopPreview()
            session.setDecoderSurface(null)
        }
        return true
    }

    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {}

    // ─── Camera preview ───────────────────────────────────────────────────

    private fun startNativeCameraPreview() {
        try {
            if (local_preview_.isAvailable) {
                val result = session.startPreview(Surface(local_preview_.surfaceTexture)) ?: -1
                if (result != 0) {
                    Log.e(TAG, "startPreview() failed: $result")
                    return
                }
            }
            Log.d(TAG, "Camera preview started")
        } catch (e: Exception) {
            Log.e(TAG, "Camera preview failed: ${e.message}")
        }
    }

    private fun switchCamera() {
        val config = getVideoEncodeConfig()
        mainSurfaceTexture?.setDefaultBufferSize(config.getWidth(), config.getHeight())
        session.switchCamera()
    }

    // ─── Lifecycle ────────────────────────────────────────────────────────

    override fun onDestroyView() {
        super.onDestroyView()
        buttonHomeMenu = null
    }

    // ─── WS callbacks ─────────────────────────────────────────────────────

    private fun onWSConnected() {}
    private fun onWSMessage(text: String) {
        val json = JSONObject(text)
        if (json.getInt("code") == WebSocketManager.HPACKLOGINUSERINFO) {
            if (json.getInt("status") == 0) {
                requireActivity().runOnUiThread {
                    appendLog(json.getString("msg"))
                    appendLog("长按日志 复制当前日志！！！")
                }
            } else appendLog("登录失败！")
        }
    }
    private fun onWSDisconnected(code: Int, reason: String) {}
    private fun onWSError(error: Throwable) {
        Log.d(TAG, "onWSError state =${error.message} ")
    }
    private fun onWSLogs(txt: String) { appendLog(txt) }

    fun getIceCandidateTypeDesc(): String {
        val type = session.getIceCandidateType() ?: 0
        return "连接模式(${if (type == 1) "P2P直连" else "Relay中转"})"
    }

    fun onDataChannelCallback(type: Int, binary: Int, data: ByteArray, size: Int) {
        if (type == 1) {
            session.sendDataChannelMsg(false, "Hello EasyRTC!!!".toByteArray(Charsets.UTF_8))
        } else if (type == 2 && binary == 0) {
            requireActivity().runOnUiThread { appendLog(data.toString(Charsets.UTF_8)) }
        }
    }

    private fun appendLog2(message: String) { AppLogStore.appendRaw("$message \n") }
    private fun appendLog(message: String) { AppLogStore.appendTimestamped(message) }

    private fun updateOnlineUsers(users: List<EasyRTCUser>) { userList = users }

    private fun showHomeMenu(anchor: View) {
        PopupMenu(requireContext(), anchor).apply {
            menuInflater.inflate(R.menu.home_overflow_menu, menu)
            setOnMenuItemClickListener { item ->
                when (item.itemId) {
                    R.id.action_home_users -> { openUserList(); true }
                    R.id.action_home_logs -> {
                        HomeLogsBottomSheetFragment().show(childFragmentManager, "home_logs"); true
                    }
                    R.id.action_home_settings -> { openSettings(); true }
                    else -> false
                }
            }
            show()
        }
    }

    private fun openUserList() {
        requireActivity().supportFragmentManager.beginTransaction()
            .replace(R.id.fragment_container, HubFragment())
            .addToBackStack("home_users")
            .commit()
    }

    private fun openSettings() { (activity as? MainActivity)?.openSettingsScreen() }

    fun sendTextMessage(text: String) {
        val message = text.trim()
        if (message.isEmpty()) return
        appendLog(message)
        session.sendDataChannelMsg(false, message.toByteArray(Charsets.UTF_8))
    }

    private fun resetCallUI() {
        local_preview_.translationX = 0f
        local_preview_.translationY = 0f
        local_preview_.setOnTouchListener(null)
        bandwidthTV?.text = ""
    }
}
