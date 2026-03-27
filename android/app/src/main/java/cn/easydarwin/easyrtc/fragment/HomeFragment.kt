package cn.easydarwin.easyrtc.fragment

import android.Manifest
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.pm.PackageManager
import cn.easyrtc.model.VideoEncodeConfig
import android.graphics.SurfaceTexture
import android.hardware.Camera
import android.media.MediaFormat
import android.os.Bundle
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
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easydarwin.easyrtc.utils.WebSocketManager
import cn.easyrtc.helper.AudioHelper
import cn.easyrtc.helper.CameraHelper
import cn.easyrtc.helper.MagicFileHelper
import cn.easyrtc.helper.RemoteRTCAudioHelper
import cn.easyrtc.helper.RemoteRTCHelper
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale


class HomeFragment : Fragment(), TextureView.SurfaceTextureListener, CameraHelper.CameraListener, AudioHelper.OnAudioDataListener, WebSocketManager.WebSocketCallback,
    EasyRTCSdk.EasyRTCEventListener {

    companion object {
        private const val TAG = "HomeFragment"
    }

    private lateinit var endCallButton: ImageButton
    private lateinit var switchCameraButton: ImageButton

    // 视频视图
    private lateinit var mainVideoView: TextureView
    private lateinit var tvFragmentUUID: TextView
    private lateinit var smallVideoView: TextureView
    private lateinit var smallVideoContainer: View

    // 摄像头帮助类 - 用于主预览
    private var mainCameraHelper: CameraHelper? = null
    private var remoteRTCHelper: RemoteRTCHelper? = null

    // 当前显示状态
    private var isMainViewShowingLocal = true // 默认主屏显示本地预览

    // Surface 状态
    private var mainSurfaceTexture: SurfaceTexture? = null
    private var smallSurfaceTexture: SurfaceTexture? = null

    private var audioHelper: AudioHelper? = null

    // 抽屉管理器
    private lateinit var drawerManager: DrawerManager

    // 文件工具类
    private lateinit var mMagicFileHelper: MagicFileHelper
    private var mRemoteRTCAudioHelper: RemoteRTCAudioHelper? = null


    private var mWebSocketManager: WebSocketManager? = null


    private var tvLogs: TextView? = null
    private var logs: StringBuilder? = null
    private var mSendBtn: TextView? = null
    private var mEditText: EditText? = null
    private val MAX_LOG_LENGTH = 100 * 1024
    private var isRelease: Boolean = false


    private var hasPermissionInit = false


    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        mMagicFileHelper = MagicFileHelper.getInstance()
        return inflater.inflate(R.layout.fragment_home, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

//        mWebSocketManager = WebSocketManager(url = "wss://rts.easyrtc.cn:19001", token = "", callback = this)
        mWebSocketManager = WebSocketManager(url = "ws://rts.easyrtc.cn:19000", token = "", callback = this)
        mWebSocketManager?.connect()


        //通话触发
        drawerManager = DrawerManager(requireContext(), view) { user ->
            Toast.makeText(requireContext(), "正在连接 ${user.username}", Toast.LENGTH_LONG).show()
            mWebSocketManager?.call(user.uuid)
        }

        //初始化 SDK
        EasyRTCSdk.getInstance();
        EasyRTCSdk.setEventListener(this)

        initViews(view)

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


        // 设置 SurfaceTexture 监听
        mainVideoView.surfaceTextureListener = this
        smallVideoView.surfaceTextureListener = this


        smallVideoContainer.setOnClickListener {

//            isMainViewShowingLocal = !isMainViewShowingLocal
            //先释放资源
//            mainCameraHelper?.releaseCamera()
//            remoteRTCHelper?.releaseVideoHandler()
//
//            val config = getVideoEncodeConfig()
//
//            mainCameraHelper = CameraHelper(requireContext(), config)
//
//            mainCameraHelper?.openCamera(if (isMainViewShowingLocal) mainSurfaceTexture else smallSurfaceTexture, this)
//
//            remoteRTCHelper = RemoteRTCHelper(Surface(if (isMainViewShowingLocal) smallSurfaceTexture else mainSurfaceTexture))

        }

        endCallButton.setOnClickListener {
            Log.d(TAG, "挂电话")
            endCallButton.visibility = View.INVISIBLE

            stopEasyRTC()

            EasyRTCSdk.release()
            appendLog("连接断开")
        }

        switchCameraButton.setOnClickListener {
            switchCamera()
        }

        // 抽屉视图已经在 DrawerManager 构造函数中自动初始化
        Log.d(TAG, "视图初始化完成")


        //TODO 消息
        tvLogs!!.setMovementMethod(ScrollingMovementMethod())

        //发送消息
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
        val useHevc = SPUtil.Companion.getInstance().getIsHevc()
        val cameraId = SPUtil.Companion.getInstance().cameraId;
        val resolution = SPUtil.Companion.getInstance().getVideoResolution()
        val bitRate = SPUtil.Companion.getInstance().getVideoBitRateKbps()
        val frameRate = SPUtil.Companion.getInstance().getVideoFrameRate()

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

                // 初始化 RemoteRTCHelper
                remoteRTCHelper = RemoteRTCHelper(Surface(surface), MediaFormat.MIMETYPE_VIDEO_AVC, 720, 1280)
                Log.d(TAG, "小窗口 SurfaceTexture 已创建")
                // 设置远程视频渲染表面
            }
        }

        // 当两个 SurfaceTexture 都准备好后，开始摄像头预览
        if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
            if (hasCameraPermission()) {
                setupCameraPreviews()
            } else {
                Log.d(TAG, "等待权限后再初始化Camera")
            }
        }
    }

    override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) {
        Log.d(TAG, "SurfaceTexture 尺寸改变: $width x $height")

        // 可以在这里调整摄像头预览尺寸
        when {
            surface == mainVideoView.surfaceTexture -> {
                Log.d(TAG, "主视频视图尺寸改变: $width x $height")
            }

            surface == smallVideoView.surfaceTexture -> {
                Log.d(TAG, "小窗口视图尺寸改变: $width x $height")
            }
        }
    }

    override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
        Log.d(TAG, "SurfaceTexture 被销毁")
        when {
            surface == mainVideoView.surfaceTexture -> {
                mainSurfaceTexture = null
            }

            surface == smallVideoView.surfaceTexture -> {
                smallSurfaceTexture = null
            }
        }
        return true
    }

    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {
        // Surface 内容更新
    }

    /**
     * 设置摄像头预览
     */
    private fun setupCameraPreviews() {

        if (!hasCameraPermission()) {
            Log.e(TAG, "没有相机权限，跳过初始化")
            return
        }

        // 主屏幕显示摄像头预览
        startMainCameraPreview()
        isMainViewShowingLocal = true

    }

    /**
     * 开始主摄像头预览
     */
    private fun startMainCameraPreview() {
        try {
            val config = getVideoEncodeConfig()
            mainCameraHelper = CameraHelper(requireContext(), config)
            mainCameraHelper?.openCamera(mainSurfaceTexture!!, this)
            Log.d(TAG, "开始主摄像头预览")
        } catch (e: Exception) {
            Log.e(TAG, "启动主摄像头失败: ${e.message}")
        }
    }

    /**
     * 切换摄像头（前后置）
     */
    private fun switchCamera(): Boolean {
        Log.d(TAG, "切换视频显示模式")
        return try {
            mainCameraHelper?.switchCamera(if (isMainViewShowingLocal) mainSurfaceTexture else smallSurfaceTexture, this)
            true
        } catch (e: Exception) {
            Log.e(TAG, "切换摄像头失败: ${e.message}")
            false
        }
    }


    private fun stopCameraPreviews() {
        // 停止主摄像头预览
        mainCameraHelper?.release()
        mainCameraHelper = null

        remoteRTCHelper?.release()
        remoteRTCHelper = null

    }

    override fun onCameraOpened(camera: Camera) {
        Log.d(TAG, "摄像头已打开")
    }

    /**
     * CameraHelper 回调 - 摄像头错误
     */
    override fun onCameraError(error: String) {
        Log.e(TAG, "摄像头错误: $error")
    }

    override fun sendVideoFrame(data: ByteArray, length: Int, frameType: Int, pts: Long) {
        EasyRTCSdk.sendVideoFrame(data, frameType, pts)
    }


    /**
     *  生命周期
     */
    override fun onPause() {
        super.onPause()
        val tag = (activity as? MainActivity)?.cFragmentTag
        if (tag !== "home") {
            stopCameraPreviews()
            remoteRTCHelper?.release()
            remoteRTCHelper = null

            // 停止音频录制
            stopEasyRTC()

            EasyRTCSdk.release()
            endCallButton.visibility = View.INVISIBLE

            isRelease = true
        }
    }

    override fun onResume() {
        super.onResume()
        Log.d(TAG, "onResume $tag")
        if (isRelease) {
            if (mainSurfaceTexture != null && smallSurfaceTexture != null) {
                setupCameraPreviews()
                remoteRTCHelper = RemoteRTCHelper(Surface(if (isMainViewShowingLocal) smallSurfaceTexture else mainSurfaceTexture))
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        Log.d(TAG, "onDestroyView")
        stopCameraPreviews()
        // 彻底停止音频录制

        stopEasyRTC()

        EasyRTCSdk.release()
    }

    override fun onHiddenChanged(hidden: Boolean) {
        super.onHiddenChanged(hidden)
        if (hidden) {
            Log.d(TAG, "Fragment 被隐藏")
            onPause()
        } else {
            Log.d(TAG, "Fragment 被显示")
            onResume()
        }
    }

    override fun onAudioData(data: ByteArray, pts: Long) {
        EasyRTCSdk.sendAudioFrame(data, pts)
    }

    override fun onError(error: String) {
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
            val audioTransceiver = EasyRTCSdk.getAudioTransceiver()

            if (audioTransceiver != 0L && audioHelper == null) {
                audioHelper = AudioHelper()
                audioHelper?.setOnAudioDataListener(this)
                audioHelper?.start()
            }

            if (mRemoteRTCAudioHelper == null) {
                mRemoteRTCAudioHelper = RemoteRTCAudioHelper(requireContext())
            }

            if (remoteRTCHelper == null) {
                remoteRTCHelper = RemoteRTCHelper(Surface(if (isMainViewShowingLocal) smallSurfaceTexture else mainSurfaceTexture))
            }

            appendLog2("------------------------------")
            appendLog2("------------------------------")
            appendLog("连接成功")
            appendLog(getIceCandidateTypeDesc())

            requireActivity().runOnUiThread {
                endCallButton.visibility = View.VISIBLE
                tvFragmentUUID.text = "${SPUtil.getInstance().rtcUserUUID} [已连接]"
            }

        } else if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_FAILED) {
            appendLog("连接失败")
            requireActivity().runOnUiThread {
                tvFragmentUUID.text = "${SPUtil.getInstance().rtcUserUUID} [连接失败]"
                endCallButton.visibility = View.INVISIBLE
            }
        } else if (state == EasyRTCPeerConnectionState.EASYRTC_PEER_CONNECTION_STATE_CLOSED) {
            appendLog("连接断开")
            stopEasyRTC()
            requireActivity().runOnUiThread {
                tvFragmentUUID.text = "${SPUtil.getInstance().rtcUserUUID} [已断开]"
                endCallButton.visibility = View.INVISIBLE
            }
        }
    }


    override fun onSDPCallback(isOffer: Int, sdp: String) {
        mWebSocketManager?.sendOfferSDP(sdp, isOffer == 1)
        if (isOffer == 1) appendLog(sdp) else {
            appendLog("======= answer ====== \n $sdp")
        }

    }

    override fun onTransceiverCallback(track: Int, codecId: Int, frameType: Int, frameData: ByteArray, frameSize: Int, pts: Long) {
        if (track == EasyRTCStreamTrack.AUDIO) {
            mRemoteRTCAudioHelper?.processRemoteAudioFrameSafe(frameData, frameData.size)
//            mMagicFileHelper.saveToFile(frameData,"remote.pcm",true)
        } else if (track == EasyRTCStreamTrack.VIDEO) {
//            mMagicFileHelper.saveToFile(frameData,"996f.h264",true)
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
        if (tvLogs == null) return
        if (logs == null) logs = StringBuilder()

        logs!!.append("$message \n")

        if (logs!!.length > MAX_LOG_LENGTH) {
            logs!!.delete(0, logs!!.length - MAX_LOG_LENGTH)
        }

        // 更新 TextView
        tvLogs!!.text = logs.toString()
        tvLogs!!.post {
            val scrollAmount = tvLogs!!.layout.getLineTop(tvLogs!!.lineCount) - tvLogs!!.height
            if (scrollAmount > 0) {
                tvLogs!!.scrollTo(0, scrollAmount)
            }
        }
    }

    private fun appendLog(message: String) {
        if (tvLogs == null) return
        if (logs == null) logs = StringBuilder()
        val formatter = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
        val line = "${formatter.format(Date())}: $message\n"

        logs!!.append(line)

        if (logs!!.length > MAX_LOG_LENGTH) {
            logs!!.delete(0, logs!!.length - MAX_LOG_LENGTH)
        }

        // 更新 TextView
        tvLogs!!.text = logs.toString()
        tvLogs!!.post {
            val scrollAmount = tvLogs!!.layout.getLineTop(tvLogs!!.lineCount) - tvLogs!!.height
            if (scrollAmount > 0) {
                tvLogs!!.scrollTo(0, scrollAmount)
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
            hasPermissionInit = true
            setupCameraPreviews()
        }
    }

    private fun hasCameraPermission(): Boolean {
        return ContextCompat.checkSelfPermission(
            requireContext(), Manifest.permission.CAMERA
        ) == PackageManager.PERMISSION_GRANTED
    }

    private fun stopEasyRTC() {

        audioHelper?.stop()
        remoteRTCHelper?.release()
        mRemoteRTCAudioHelper?.release()

        audioHelper = null
        remoteRTCHelper = null
        mRemoteRTCAudioHelper = null
    }

}