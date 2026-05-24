package cn.easyrtc.model

import android.util.Size
import android.hardware.Camera
import android.util.Log
import androidx.lifecycle.MutableLiveData

sealed class LiveUiState {
    data object Idle : LiveUiState()
    data class Connected(val user: String?) : LiveUiState()
    data class Disconnected(val user: String?) : LiveUiState()
    data class Failed(val user: String?, val reason: String?) : LiveUiState()
}
object LiveSessionController: MutableLiveData<LiveUiState>(LiveUiState.Idle) {
    private var currentUser: String? = null

    fun onConnected(user: String?) {
        currentUser = user
        postValue(
            LiveUiState.Connected(
                currentUser
            )
        )
    }

    fun onClosed() {
        postValue(
            LiveUiState.Disconnected(
                currentUser
            )
        )
    }

    fun onFailed(reason: String? = null) {
        postValue(
            LiveUiState.Failed(
                currentUser,
                reason
            )
        )
    }
}

sealed class DataChannelEvent {
    data object Open : DataChannelEvent()
    data object Idle : DataChannelEvent()
    data class Message(val binary: Int, val data: ByteArray, val size: Int) : DataChannelEvent() {
        override fun equals(other: Any?): Boolean = other is Message
    }
}
object DataChannelLiveData : MutableLiveData<DataChannelEvent>(DataChannelEvent.Idle) {
    fun onOpen() { postValue(DataChannelEvent.Open) }
    fun onMessage(binary: Int, data: ByteArray, size: Int) {
        postValue(DataChannelEvent.Message(binary, data.copyOf(size), size))
    }
}
object RemoteVideoSizeLiveData : MutableLiveData<android.util.Size>(android.util.Size(0, 0))

data class VideoEncodeConfig(
    private val useHevc: Boolean = false,
    private val frameRate: Int = 25,
    private val cameraId: Int = Camera.CameraInfo.CAMERA_FACING_BACK,
    private val resolution: Size = Size(1280, 720),
    private val orientation: Int = ORIENTATION_0, //是否旋转
    private val bitRate: Int = 500_000,
    private val iFrameInterval: Int = 10,
) {

    // 验证参数的有效性
    init {
        require(frameRate > 0) { "帧率必须大于0" }
        require(iFrameInterval > 0) { "关键帧间隔必须大于0" }
        require(bitRate > 0) { "比特率必须大于0" }
        require(resolution.width > 0 && resolution.height > 0) { "分辨率必须大于0" }
        Log.d(TAG, toString())
    }

    fun getCameraId(): Int = cameraId

    // 获取视频宽度
    fun getWidth(): Int = resolution.width

    // 获取视频高度
    fun getHeight(): Int = resolution.height

    fun getBitRate(): Int = bitRate

    fun getIFrameInterval(): Int = iFrameInterval

    fun getOrientation(): Int = orientation

    fun getMimeType(): String {
        return if (useHevc) "video/hevc" else "video/avc"
    }

    fun getFrameRate(): Int = frameRate

    fun getUseHevc(): Boolean = useHevc


    // 复制并修改配置
    fun copyWith(
        useHevc: Boolean = this.useHevc, frameRate: Int = this.frameRate, iFrameInterval: Int = this.iFrameInterval, bitRate: Int = this.bitRate, resolution: Size = this.resolution
    ): VideoEncodeConfig {
        return VideoEncodeConfig(
            useHevc = useHevc, frameRate = frameRate, iFrameInterval = iFrameInterval, bitRate = bitRate, resolution = resolution
        )
    }

    override fun toString(): String {
        return "VideoEncodeConfig(useHevc=$useHevc, mimeType='${getMimeType()}', frameRate=$frameRate, iFrameInterval=$iFrameInterval, bitRate=$bitRate, resolution=$resolution)"
    }


    companion object {
        private const val TAG = "VideoEncodeConfig"
        // 定义允许的旋转角度常量
        const val ORIENTATION_0 = 0
        const val ORIENTATION_90 = 90
    }
}