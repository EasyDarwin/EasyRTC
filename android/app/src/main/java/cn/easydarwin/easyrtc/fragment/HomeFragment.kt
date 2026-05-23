package cn.easydarwin.easyrtc.fragment

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.SurfaceTexture
import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.util.Log
import android.view.MotionEvent
import android.view.LayoutInflater
import android.view.Surface
import android.view.TextureView
import android.view.View
import android.view.ViewGroup
import android.media.AudioManager
import android.widget.ImageButton
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.widget.PopupMenu
import cn.easydarwin.easyrtc.MainActivity
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.service.WebSocketService
import cn.easyrtc.EasyRTCUser
import cn.easyrtc.EasyRTCPeerConnectionState
import cn.easyrtc.EasyRTCSdk
import cn.easyrtc.EasyRTCStreamTrack
import cn.easyrtc.helper.MagicFileHelper
import cn.easyrtc.media.MediaSession
import cn.easydarwin.easyrtc.ui.live.BaseRtcMediaFragment
import cn.easydarwin.easyrtc.ui.hub.HubFragment
import cn.easydarwin.easyrtc.ui.live.LiveSessionController
import cn.easydarwin.easyrtc.ui.live.LiveUiState
import cn.easydarwin.easyrtc.ui.live.NativePipelineController
import cn.easydarwin.easyrtc.ui.live.NativePipelineState
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easydarwin.easyrtc.utils.WebSocketManager
import org.json.JSONObject
import java.util.Locale

class HomeFragment : BaseRtcMediaFragment(), TextureView.SurfaceTextureListener,
    EasyRTCSdk.EasyRTCEventListener {

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
    private var isRelease: Boolean = false

    private val pipelineController = NativePipelineController()

    private val liveSessionController = LiveSessionController()
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
                    is WebSocketService.Event.OnlineUsers -> updateOnlineUsers(event.users)
                }
            }
        }

        (activity as? MainActivity)?.incomingCallLiveData?.observe(viewLifecycleOwner) { event ->
            if (event.handled) return@observe
            activeSessionUser = event.uuid
            tvFragmentUUID.text = "来电: ${event.uuid}"
            appendLog("来电: ${event.uuid}")
            view.post{
                webSocketService?.handleIncomingCall(event, session);
            }
        }

        EasyRTCSdk.getInstance()
        EasyRTCSdk.setEventListener(this)

        initViews(view)
        observeLiveSessionState()
    }

    private fun observeLiveSessionState() {
        liveSessionController.state.observe(viewLifecycleOwner) { state ->
            when (state) {
                is LiveUiState.Idle -> {
                    requireActivity().runOnUiThread {
                        tvFragmentUUID.text = SPUtil.getInstance().rtcUserUUID
                        endCallButton.visibility = View.GONE
                    }
                }

                is LiveUiState.Connected -> {
                    appendLog2("------------------------------")
                    appendLog2("------------------------------")
                    appendLog("连接成功")
                    appendLog(getIceCandidateTypeDesc())

                    requireActivity().runOnUiThread {
                        endCallButton.visibility = View.VISIBLE
                        tvFragmentUUID.text = "${SPUtil.getInstance().rtcUserUUID} [已连接]"
                    }
                }

                is LiveUiState.Disconnected -> {
                    appendLog("连接断开")
                    stopEasyRTC()
                    requireActivity().runOnUiThread {
                        tvFragmentUUID.text = "${SPUtil.getInstance().rtcUserUUID} [已断开]"
                        endCallButton.visibility = View.INVISIBLE
                    }
                }

                is LiveUiState.Failed -> {
                    appendLog("连接失败")
                    state.reason?.takeIf { it.isNotBlank() }?.let { appendLog("失败原因: $it") }
                    stopEasyRTC()
                    requireActivity().runOnUiThread {
                        tvFragmentUUID.text = "${SPUtil.getInstance().rtcUserUUID} [连接失败]"
                        endCallButton.visibility = View.INVISIBLE
                    }
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
//            endCallButton.visibility = View.INVISIBLE
//            stopEasyRTC()
//            val session = EasyRTCSdk.getMediaSession()
//            session.stopPreview()
//            EasyRTCSdk.release()
//            liveSessionController.onClosed()
            (activity as? MainActivity)?.apply {
//                switchFragment(if (lastFragment != null) lastFragment!! else "hub")
                switchFragment("", true);
                switchFragment("p2p_call")
            }
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

        Log.d(TAG, "视图初始化完成")
    }

    override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
        Log.d(TAG, "SurfaceTexture 可用: $width x $height")

        when (surface) {
            local_preview_.surfaceTexture -> {
                mainSurfaceTexture = surface
                // Use LANDSCAPE buffer so camera picks a standard resolution (matches encoder path)
                val config = getVideoEncodeConfig()
                surface.setDefaultBufferSize(config.getWidth(), config.getHeight())  // 1280×720 landscape
                Log.d(TAG, "主视频 SurfaceTexture 已创建, buffer=${config.getWidth()}x${config.getHeight()}")
            }

            remote_preview_.surfaceTexture -> {
                smallSurfaceTexture = surface
                session.setupVideoEncoder(getVideoEncodeConfig())
                session.setDecoderSurface(Surface(surface))
                Log.d(TAG, "小窗口 SurfaceTexture 已创建")
            }
        }

        if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
            if (hasCameraPermission()) {
                startNativeCameraPreview()
            } else {
                Log.d(TAG, "等待权限后再初始化Camera")
            }
        }
    }

    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {
        Log.d(TAG, "SurfaceTexture 尺寸改变: $width x $height")
    }

    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
        Log.d(TAG, "SurfaceTexture 被销毁")
        when {
            surface == local_preview_.surfaceTexture -> mainSurfaceTexture = null
            surface == remote_preview_.surfaceTexture -> {
                smallSurfaceTexture = null
                session.setDecoderSurface(null)
            }
        }
        return true
    }

    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {
    }

    private fun startNativeCameraPreview() {
        if (!hasCameraPermission()) {
            Log.e(TAG, "没有相机权限，跳过初始化")
            return
        }

        try {

            if (local_preview_.isAvailable) {
                val result = session.startPreview(Surface(local_preview_.surfaceTexture))
                if (result != 0) {
                    Log.e(TAG, "startPreview() failed: $result")
                    pipelineController.reportError("startPreview failed: $result")
                    return
                }
            }

            pipelineController.start()
            Log.d(TAG, "Camera preview started")
        } catch (e: Exception) {
            Log.e(TAG, "Camera preview failed: ${e.message}")
            pipelineController.reportError(e.message ?: "unknown")
        }
    }

    private fun switchCamera() {
        val state = pipelineController.state
        if (state !is NativePipelineState.Running) {
            Log.w(TAG, "Cannot switch camera: pipeline not running")
            return
        }
        session.switchCamera()
        pipelineController.switchCamera()
        Log.d(TAG, "Camera switched")
    }

    override fun onPause() {
        super.onPause()
        handleHiddenResources()
    }

    override fun onResume() {
        super.onResume()
        Log.d(TAG, "onResume $tag")
        handleVisibleResources()
    }

    override fun onDestroyView() {
        Log.d(TAG, "onDestroyView")
        EasyRTCSdk.unsetEventListener(this)
        stopEasyRTC()
        session.stopPreview()
        super.onDestroyView()
        buttonHomeMenu = null
    }

    override fun onHiddenChanged(hidden: Boolean) {
        super.onHiddenChanged(hidden)
        if (hidden) {
            Log.d(TAG, "Fragment 被隐藏")
            handleHiddenResources()
        } else {
            Log.d(TAG, "Fragment 被显示")
            handleVisibleResources()
        }
    }

    private fun handleHiddenResources() {
        val tag = (activity as? MainActivity)?.cFragmentTag
        if (tag != "p2p_call") {
            stopEasyRTC()
            endCallButton.visibility = View.INVISIBLE
            isRelease = true
        }
    }

    private fun handleVisibleResources() {
        if (isRelease) {
            if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
                startNativeCameraPreview()
            }
        }
    }

    private fun onWSConnected() {
    }

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

    private fun onWSDisconnected(code: Int, reason: String) {
    }

    private fun onWSError(error: Throwable) {
        Log.d(TAG, "onWSError state =${error.message} ")
    }

    private fun onWSLogs(txt: String) {
        appendLog(txt)
    }

    fun getIceCandidateTypeDesc(): String {
        val type = EasyRTCSdk.getIceCandidateType()
        val desc = if (type == 1) "P2P直连" else "Relay中转"
        return "连接模式($desc)"
    }

    override fun connectionStateChange(state: Int) {
        Log.d(TAG, "connectionStateChange state =$state  ${state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CONNECTED}")
        session.setConnectState(state);
        if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CONNECTED) {
            activity?.runOnUiThread {
                session.requestKeyFrame()
                liveSessionController.onConnected(activeSessionUser ?: SPUtil.getInstance().rtcUserUUID)
            }

        } else if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_FAILED) {
            liveSessionController.onFailed()
        } else if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CLOSED) {
            liveSessionController.onClosed()
        }
    }

    override fun onSDPCallback(isOffer: Int, sdp: String) {
        webSocketService?.sendOfferSDP(sdp, isOffer == 1)
        if (isOffer == 1) appendLog("======= offer from local ====== \n $sdp") else {
            appendLog("======= answer from local ====== \n $sdp")
        }
    }

    override fun onTransceiverCallback(track: Int, codecId: Int, frameType: Int, frameData: ByteArray, frameSize: Int, pts: Long) {
    }

    override fun onRemoteVideoSize(width: Int, height: Int) {
        activity?.runOnUiThread {
            val density = resources.displayMetrics.density
            val isPortrait = height > width

            val desiredWidthPx = remote_preview_container.width
            val desiredHeightPx = desiredWidthPx * height / width
            val lp = remote_preview_container.layoutParams
            lp.width = desiredWidthPx
            lp.height = desiredHeightPx
            remote_preview_container.layoutParams = lp
            Log.d(TAG, "Remote video size: ${width}x${height}, container: ${lp.width}x${lp.height}")



            val desiredWidthDp = 120
            local_preview_.layoutParams.width = Math.round(desiredWidthDp * density)
            local_preview_.layoutParams.height = Math.round(local_preview_.layoutParams.width * 1280f/720f);
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

    override fun onDataChannelCallback(type: Int, binary: Int, data: ByteArray, size: Int) {
        if (type == 1) {
            val data1 = "Hello EasyRTC!!!".toByteArray(Charsets.UTF_8)
            EasyRTCSdk.sendMsg(0, data1)
        } else if (type == 2) {
            if (binary == 0) {
                requireActivity().runOnUiThread {
                    appendLog(data.toString(Charsets.UTF_8))
                }
            }
        }
    }

    private fun appendLog2(message: String) {
        AppLogStore.appendRaw("$message \n")
    }

    private fun appendLog(message: String) {
        AppLogStore.appendTimestamped(message)
    }

    private fun updateOnlineUsers(users: List<EasyRTCUser>) {
        userList = users
    }

    private fun showHomeMenu(anchor: View) {
        PopupMenu(requireContext(), anchor).apply {
            menuInflater.inflate(R.menu.home_overflow_menu, menu)
            setOnMenuItemClickListener { item ->
                when (item.itemId) {
                    R.id.action_home_users -> {
                        openUserList()
                        true
                    }
                    R.id.action_home_logs -> {
                        HomeLogsBottomSheetFragment().show(childFragmentManager, "home_logs")
                        true
                    }
                    R.id.action_home_settings -> {
                        openSettings()
                        true
                    }
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

    private fun callUser(uuid: String) {
        appendLog("呼叫: $uuid")
        webSocketService?.call(uuid)
    }

    private fun openSettings() {
        (activity as? MainActivity)?.openSettingsScreen()
    }

    fun sendTextMessage(text: String) {
        val message = text.trim()
        if (message.isEmpty()) return
        appendLog(message)
        EasyRTCSdk.sendMsg(0, message.toByteArray(Charsets.UTF_8))
    }

    fun onPermissionGranted() {
        Log.d(TAG, "权限已授予，重新初始化Camera")
        if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
            startNativeCameraPreview()
        }
    }

    private fun stopEasyRTC() {
        session.stopSend()
        EasyRTCSdk.release()
        EasyRTCSdk.bindMediaSession(session)
        pipelineController.stop()
    }

}
