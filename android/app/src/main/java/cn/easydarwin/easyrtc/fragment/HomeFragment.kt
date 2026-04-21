package cn.easydarwin.easyrtc.fragment

import android.Manifest
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.SurfaceTexture
import android.media.MediaFormat
import cn.easyrtc.model.VideoEncodeConfig
import android.os.Bundle
import android.os.Looper
import android.text.method.ScrollingMovementMethod
import android.util.Log
import android.view.LayoutInflater
import android.view.Surface
import android.view.TextureView
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.ImageButton
import android.widget.TextView
import android.widget.Toast
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import cn.easydarwin.easyrtc.MainActivity
import cn.easydarwin.easyrtc.R
import cn.easyrtc.EasyRTCPeerConnectionState
import cn.easyrtc.EasyRTCSdk
import cn.easyrtc.EasyRTCStreamTrack
import cn.easyrtc.EasyRTCUser
import cn.easydarwin.easyrtc.modal.DrawerManager
import cn.easydarwin.easyrtc.ui.live.LiveSessionController
import cn.easydarwin.easyrtc.ui.live.LiveUiState
import cn.easydarwin.easyrtc.ui.live.NativePipelineController
import cn.easydarwin.easyrtc.ui.live.NativePipelineState
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easydarwin.easyrtc.utils.WebSocketManager
import cn.easyrtc.helper.MagicFileHelper
import cn.easyrtc.helper.RemoteRTCHelper
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale


class HomeFragment : Fragment(), TextureView.SurfaceTextureListener, WebSocketManager.WebSocketCallback,
    EasyRTCSdk.EasyRTCEventListener {

    companion object {
        private const val TAG = "HomeFragment"
    }

    private lateinit var endCallButton: ImageButton
    private lateinit var switchCameraButton: ImageButton

    private lateinit var mainVideoView: TextureView
    private lateinit var tvFragmentUUID: TextView
    private lateinit var smallVideoView: TextureView
    private lateinit var smallVideoContainer: View

    private var remoteRTCHelper: RemoteRTCHelper? = null
    private var isMainViewShowingLocal = true

    private var mainSurfaceTexture: SurfaceTexture? = null
    private var smallSurfaceTexture: SurfaceTexture? = null

    private lateinit var drawerManager: DrawerManager
    private lateinit var mMagicFileHelper: MagicFileHelper

    private var mWebSocketManager: WebSocketManager? = null

    private var tvLogs: TextView? = null
    private var logs: StringBuilder? = null
    private var mSendBtn: TextView? = null
    private var mEditText: EditText? = null
    private val MAX_LOG_LENGTH = 100 * 1024
    private var isRelease: Boolean = false

    private val pipelineController = NativePipelineController()

    private val liveSessionController = LiveSessionController()
    private var activeSessionUser: String? = null


    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        mMagicFileHelper = MagicFileHelper.getInstance()
        return inflater.inflate(R.layout.fragment_home, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        mWebSocketManager = WebSocketManager(url = "ws://rts.easyrtc.cn:19000", token = "", callback = this)
        mWebSocketManager?.connect()

        drawerManager = DrawerManager(requireContext(), view) { user ->
            activeSessionUser = user.username
            Toast.makeText(requireContext(), "正在连接 ${user.username}", Toast.LENGTH_LONG).show()
            mWebSocketManager?.call(user.uuid)
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
        mainVideoView = view.findViewById(R.id.mainVideoView)
        tvFragmentUUID = view.findViewById(R.id.tvFragmentUUID)
        smallVideoView = view.findViewById(R.id.smallVideoView)
        smallVideoContainer = view.findViewById(R.id.smallVideoContainer)
        endCallButton = view.findViewById(R.id.endCallButton)
        switchCameraButton = view.findViewById(R.id.switchCameraButton)

        endCallButton.visibility = View.GONE

        tvLogs = view.findViewById(R.id.tv_logs)
        mSendBtn = view.findViewById(R.id.send_msg)
        mEditText = view.findViewById(R.id.et_msg)

        tvFragmentUUID.text = SPUtil.getInstance().rtcUserUUID

        tvLogs?.setOnLongClickListener {
            copyLogs()
            true
        }

        mainVideoView.surfaceTextureListener = this
        smallVideoView.surfaceTextureListener = this

        smallVideoContainer.setOnClickListener { }

        endCallButton.setOnClickListener {
            Log.d(TAG, "挂电话")
            endCallButton.visibility = View.INVISIBLE
            stopEasyRTC()
            EasyRTCSdk.release()
            liveSessionController.onClosed()
        }

        switchCameraButton.setOnClickListener {
            switchCamera()
        }

        Log.d(TAG, "视图初始化完成")

        tvLogs!!.setMovementMethod(ScrollingMovementMethod())

        mSendBtn!!.setOnClickListener(object : View.OnClickListener {
            override fun onClick(view: View?) {
                val text = mEditText?.getText().toString()
                if (text.isEmpty()) return
                appendLog(text)
                val data = text.toByteArray(Charsets.UTF_8)
                EasyRTCSdk.sendMsg(0, data)
                mEditText?.setText("")
            }
        })
    }

    private fun getVideoEncodeConfig(): VideoEncodeConfig {
        val useHevc = SPUtil.getInstance().getIsHevc()
        val cameraId = SPUtil.getInstance().cameraId
        val resolution = SPUtil.getInstance().getVideoResolution()
        val bitRate = SPUtil.getInstance().getVideoBitRateKbps()
        val frameRate = SPUtil.getInstance().getVideoFrameRate()
        return VideoEncodeConfig(useHevc, frameRate, cameraId, resolution, VideoEncodeConfig.ORIENTATION_90, bitRate)
    }

    override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
        Log.d(TAG, "SurfaceTexture 可用: $width x $height")

        when (surface) {
            mainVideoView.surfaceTexture -> {
                mainSurfaceTexture = surface
                Log.d(TAG, "主视频 SurfaceTexture 已创建")
            }

            smallVideoView.surfaceTexture -> {
                smallSurfaceTexture = surface
                smallSurfaceTexture?.let {
                    remoteRTCHelper = RemoteRTCHelper(Surface(it), MediaFormat.MIMETYPE_VIDEO_AVC, 720, 1280)
                }
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
            surface == mainVideoView.surfaceTexture -> mainSurfaceTexture = null
            surface == smallVideoView.surfaceTexture -> smallSurfaceTexture = null
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
            val config = getVideoEncodeConfig()
            val session = EasyRTCSdk.getMediaSession()

            val setupResult = session.setupVideoEncoder(config)
            if (setupResult != 0) {
                Log.e(TAG, "setupVideoEncoder() failed: $setupResult")
                pipelineController.reportError("setupVideoEncoder failed: $setupResult")
                return
            }

            if (mainVideoView.isAvailable) {
                mainVideoView.surfaceTexture?.let {
                    session.setPreviewSurface(Surface(it))
                }
            }

            val result = session.start()
            if (result != 0) {
                Log.e(TAG, "MediaSession.start() failed: $result")
                pipelineController.reportError("start failed: $result")
                session.stop()
                return
            }

            pipelineController.start()
            isMainViewShowingLocal = true
            Log.d(TAG, "Native camera preview started via MediaSession")
        } catch (e: Exception) {
            Log.e(TAG, "Native camera preview failed: ${e.message}")
            pipelineController.reportError(e.message ?: "unknown")
        }
    }

    private fun switchCamera() {
        val state = pipelineController.state
        if (state !is NativePipelineState.Running) {
            Log.w(TAG, "Cannot switch camera: pipeline not running")
            return
        }
        EasyRTCSdk.getMediaSession().switchCamera()
        pipelineController.switchCamera()
        Log.d(TAG, "Camera switched")
    }

    private fun stopNativeMediaPipeline() {
        EasyRTCSdk.getMediaSession().stop()
        pipelineController.stop()
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
        super.onDestroyView()
        Log.d(TAG, "onDestroyView")
        stopEasyRTC()
        mWebSocketManager?.shutdown()
        mWebSocketManager = null
        EasyRTCSdk.release()
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
        if (tag != "live") {
            stopEasyRTC()
            endCallButton.visibility = View.INVISIBLE
            isRelease = true
        }
    }

    private fun handleVisibleResources() {
        if (isRelease) {
            if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
                startNativeCameraPreview()
                ensureRemoteRTCHelper()
            }
        }
    }

    override fun onWSConnected() {
    }

    override fun onWSMessage(text: String) {
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

    override fun onWSDisconnected(code: Int, reason: String) {
    }

    override fun onWSError(error: Throwable) {
        Log.d(TAG, "onWSError state =${error.message} ")
    }

    override fun onWSOnlineUsers(users: List<EasyRTCUser>) {
        Log.d(TAG, "现在用户 = ${users.size}")
        requireActivity().runOnUiThread {
            drawerManager.updateUserListUI(users)
        }
    }

    override fun onWSLogs(txt: String) {
        appendLog(txt)
    }

    fun getIceCandidateTypeDesc(): String {
        val type = EasyRTCSdk.getIceCandidateType()
        val desc = if (type == 1) "P2P直连" else "Relay中转"
        return "连接模式($desc)"
    }

    override fun connectionStateChange(state: Int) {
        Log.d(TAG, "connectionStateChange state =$state  ${state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CONNECTED}")
        if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CONNECTED) {
            val session = EasyRTCSdk.getMediaSession()
            session.requestKeyFrame()

            ensureRemoteRTCHelper()
            liveSessionController.onConnected(activeSessionUser ?: SPUtil.getInstance().rtcUserUUID)

        } else if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_FAILED) {
            liveSessionController.onFailed()
        } else if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CLOSED) {
            liveSessionController.onClosed()
        }
    }

    override fun onSDPCallback(isOffer: Int, sdp: String) {
        mWebSocketManager?.sendOfferSDP(sdp, isOffer == 1)
        if (isOffer == 1) appendLog(sdp) else {
            appendLog("======= answer ====== \n $sdp")
        }
    }

    override fun onTransceiverCallback(track: Int, codecId: Int, frameType: Int, frameData: ByteArray, frameSize: Int, pts: Long) {
        if (track == EasyRTCStreamTrack.VIDEO) {
            remoteRTCHelper?.onRemoteVideoFrame(frameData)
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
        runOnMainThread {
            if (tvLogs == null) return@runOnMainThread
            if (logs == null) logs = StringBuilder()
            logs!!.append("$message \n")
            if (logs!!.length > MAX_LOG_LENGTH) {
                logs!!.delete(0, logs!!.length - MAX_LOG_LENGTH)
            }
            tvLogs!!.text = logs.toString()
            tvLogs!!.post {
                val scrollAmount = tvLogs!!.layout.getLineTop(tvLogs!!.lineCount) - tvLogs!!.height
                if (scrollAmount > 0) {
                    tvLogs!!.scrollTo(0, scrollAmount)
                }
            }
        }
    }

    private fun appendLog(message: String) {
        runOnMainThread {
            if (tvLogs == null) return@runOnMainThread
            if (logs == null) logs = StringBuilder()
            val formatter = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
            val line = "${formatter.format(Date())}: $message\n"
            logs!!.append(line)
            if (logs!!.length > MAX_LOG_LENGTH) {
                logs!!.delete(0, logs!!.length - MAX_LOG_LENGTH)
            }
            tvLogs!!.text = logs.toString()
            tvLogs!!.post {
                val scrollAmount = tvLogs!!.layout.getLineTop(tvLogs!!.lineCount) - tvLogs!!.height
                if (scrollAmount > 0) {
                    tvLogs!!.scrollTo(0, scrollAmount)
                }
            }
        }
    }

    private fun copyLogs() {
        val cm = requireActivity().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val text = tvLogs?.text.toString()
        cm.setPrimaryClip(ClipData.newPlainText("logs", text))
        Toast.makeText(requireActivity(), "日志已复制", Toast.LENGTH_SHORT).show()
    }

    fun onPermissionGranted() {
        Log.d(TAG, "权限已授予，重新初始化Camera")
        if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
            startNativeCameraPreview()
        }
    }

    private fun ensureRemoteRTCHelper() {
        if (remoteRTCHelper != null) return
        val renderSurfaceTexture = if (isMainViewShowingLocal) smallSurfaceTexture else mainSurfaceTexture
        if (renderSurfaceTexture == null) {
            Log.w(TAG, "远端渲染 SurfaceTexture 未就绪，跳过 RemoteRTCHelper 初始化")
            return
        }
        remoteRTCHelper = RemoteRTCHelper(Surface(renderSurfaceTexture))
    }

    private fun stopEasyRTC() {
        stopNativeMediaPipeline()
        remoteRTCHelper?.release()
        remoteRTCHelper = null
    }

    private fun runOnMainThread(action: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            action()
        } else {
            activity?.runOnUiThread(action) ?: view?.post(action)
        }
    }

    private fun hasCameraPermission(): Boolean {
        return ContextCompat.checkSelfPermission(
            requireContext(), Manifest.permission.CAMERA
        ) == PackageManager.PERMISSION_GRANTED
    }
}
