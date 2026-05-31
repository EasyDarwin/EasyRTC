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
import android.widget.ImageButton
import android.widget.TextView
import androidx.appcompat.widget.PopupMenu
import androidx.preference.PreferenceManager
import cn.easydarwin.easyrtc.MainActivity
import cn.easydarwin.easyrtc.R
import cn.easydarwin.easyrtc.fragment.HomeFragment
import cn.easyrtc.EasyRTCCodec
import cn.easyrtc.EasyRTCPeerConnectionState
import cn.easyrtc.whip.WhipModule
import cn.easydarwin.easyrtc.ui.live.BaseRtcMediaFragment
import cn.easydarwin.easyrtc.utils.AppLogStore
import cn.easydarwin.easyrtc.utils.SPUtil
import cn.easyrtc.model.DataChannelEvent

class WhipFragment : BaseRtcMediaFragment(), TextureView.SurfaceTextureListener{

    companion object {
        private const val TAG = "WhipFragment"
        private const val DEFAULT_WHIP_URL = "https://demo.easygbs.com:10010/live/test0508_01.whip"
        private const val PREF_WHIP_SERVER = "pref_whip_server"

        fun newInstance(): WhipFragment = WhipFragment()
    }

    private var whipModule: WhipModule? = null
    private var isRunning = false
    private var isLocalPreviewStarted = false
    private lateinit var localPreview: TextureView
    private var localSurfaceTexture: SurfaceTexture? = null

    private lateinit var tvStatus: TextView
    private lateinit var btnToggleStream: Button
    private lateinit var buttonWhipMenu: ImageButton
    private lateinit var tvWhipServer: TextView

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

        tvStatus = view.findViewById(R.id.tv_status)
        btnToggleStream = view.findViewById(R.id.btn_toggle_stream)
        buttonWhipMenu = view.findViewById(R.id.button_whip_menu)
        tvWhipServer = view.findViewById(R.id.tvWhipServer)
        localPreview = view.findViewById(R.id.local_preview_)
        localPreview.post {
            localPreview.layoutParams.height = localPreview.layoutParams.width * cameraVideoHeight / cameraVideoWidth
        }

        tvStatus.movementMethod = ScrollingMovementMethod()
        localPreview.surfaceTextureListener = this

        updateServerAddressDisplay()

        buttonWhipMenu?.setOnClickListener {
            showWhipMenu(it)
        }

        btnToggleStream.setOnClickListener { 
            if (isRunning) stopWhipPush() else startWhipPush() 
        }

        updateButtonState()

    }

    /** Camera video size — landscape (native sensor) dimensions. */
    private var cameraVideoWidth = 1280
    private var cameraVideoHeight = 720

    private fun updateServerAddressDisplay() {
        val sharedPrefs = PreferenceManager.getDefaultSharedPreferences(requireContext())
        val serverAddr = sharedPrefs.getString(PREF_WHIP_SERVER, DEFAULT_WHIP_URL) ?: DEFAULT_WHIP_URL
        tvWhipServer.text = serverAddr
    }

    private fun getWhipServerUrl(): String {
        val sharedPrefs = PreferenceManager.getDefaultSharedPreferences(requireContext())
        return sharedPrefs.getString(PREF_WHIP_SERVER, DEFAULT_WHIP_URL) ?: DEFAULT_WHIP_URL
    }

    private fun showWhipMenu(anchor: View) {
        PopupMenu(requireContext(), anchor).apply {
            menu.add(0, 1, 0, "设置")
            setOnMenuItemClickListener { item ->
                when (item.itemId) {
                    1 -> {
                        openWhipSettings()
                        true
                    }
                    2 -> {
                        // Could open a logs dialog here
                        true
                    }
                    else -> false
                }
            }
            show()
        }
    }

    private fun openWhipSettings() {
        (activity as? MainActivity)?.openSettingsScreen()
    }

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
        val url = getWhipServerUrl().trim()
        if (url.isEmpty()) {
            appendLog("请先在设置中配置 WHIP 推流地址")
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

            session.createPeerConnection("", "", "", "")
            val encodeConfig = getVideoEncodeConfig()
            cameraVideoWidth = encodeConfig.getWidth()
            cameraVideoHeight = encodeConfig.getHeight()
            session.setupVideoEncoder(encodeConfig)
            session.addTransceivers(
                videoCodec = if (SPUtil.getInstance().getIsHevc()) EasyRTCCodec.H265 else EasyRTCCodec.H264,
                audioCodec = EasyRTCCodec.ALAW
            )
            appendLog("创建 Offer SDP...")
            session.createOffer { sdp ->
                appendLog("发送 Offer SDP 到服务器...")
                whipModule?.postOffer(
                    offerSdp = sdp,
                    onSuccess = { answerSdp ->
                        appendLog("收到 Answer SDP，长度: ${answerSdp.length}")
                        runOnMainThread {
                            session.setRemoteDescription(answerSdp)
                            appendLog("设置远端描述完成")
                        }
                    },
                    onError = { error ->
                        appendLog(error)
                    }
                )
            }

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
        whipModule?.cancel();
        whipModule = null
        session.releasePeerConnection()
        appendLog("=== 推流已停止 ===")
    }


    fun onDataChannelCallback(type: Int, binary: Int, data: ByteArray, size: Int) {
    }

    fun onRemoteVideoSize(width: Int, height: Int) {
        // WHIP 推流仅发送，不接收远端视频
    }

    override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
        if (surface == localPreview.surfaceTexture) {
            val config = getVideoEncodeConfig()
            surface.setDefaultBufferSize(config.getWidth(), config.getHeight())
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
            session.releasePeerConnection()
            session.stopPreview()
            isRunning = false
            isLocalPreviewStarted = false
        }
        return true
    }

    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {
    }

    private fun startLocalPreviewIfAvailable() {
        if (localSurfaceTexture == null || !localPreview.isAvailable) return
        if (isLocalPreviewStarted) {
            AppLogStore.appendCritical(TAG, "startLocalPreviewIfAvailable skipped: preview already running")
            return
        }
        val result = session.startPreview(Surface(localSurfaceTexture)) ?: -1
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

    private fun appendLog(message: String) {
        AppLogStore.appendTimestamped("[WHIP][$TAG] $message")
        runOnMainThread {
            tvStatus.append(message)
            tvStatus.append("\n")
        }
    }

    private fun updateButtonState() {
        btnToggleStream.text = if (isRunning) "停止推流" else "开始推流"
    }

    override fun onResume() {
        super.onResume()
        updateServerAddressDisplay()
    }

    override fun onDestroyView() {
        AppLogStore.appendCritical(TAG, "onDestroyView: releasing WHIP resources, isRunning=$isRunning")
        if (isRunning) {
            whipModule?.cancel()
            whipModule = null
            isRunning = false
            isLocalPreviewStarted = false
        }
        super.onDestroyView()
    }

    override fun onDestroy() {
        whipModule = null
        super.onDestroy()
    }
}
