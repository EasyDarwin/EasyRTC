package cn.easyrtc

import android.os.Handler
import android.os.Looper
import android.util.Log
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

data class EasyRTCUser(val uuid: String, val username: String)

object EasyRTCCodec {
    const val H264 = 1
    const val OPUS = 2
    const val VP8 = 3
    const val MULAW = 4
    const val ALAW = 5
    const val H265 = 6
    const val AAC = 7
    const val UNKNOWN = 8
}

object EasyRTCStreamTrack {
    const val AUDIO = 1   //  音频轨,
    const val VIDEO = 2    // 视频轨
}


object EasyRTCDirection {
    const val EasyRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV = 1      // 发送&接收
    const val EaSyRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY = 2      // 仅发送
    const val EaSyRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY = 3      // 仅接收
    const val EasyRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE = 4      // 对端无法发送或接收数据;
}

object EasyRTCPeerConnectionState {
    const val EASYRTC_PEER_CONNECTION_STATE_NONE = 0            //初始状态
    const val EASYRTC_PEER_CONNECTION_STATE_NEW = 1             //当ICE代理等待远程凭证时设置此状态
    const val EASYRTC_PEER_CONNECTION_STATE_CONNECTING = 2      //当ICE代理检查连接时设置此状态         连接中
    const val EASYRTC_PEER_CONNECTION_STATE_CONNECTED = 3       //当ICE代理就绪时设置此状态            连接成功
    const val EASYRTC_PEER_CONNECTION_STATE_DISCONNECTED = 4    //当ICE代理断开连接时设置此状态         连接断开
    const val EASYRTC_PEER_CONNECTION_STATE_FAILED = 5          //当ICE代理转换为失败状态时设置此状态    连接失败
    const val EASYRTC_PEER_CONNECTION_STATE_CLOSED = 6         //此状态导致流会话终止                  连接断开
    const val EASYRTC_PEER_CONNECTION_TOTAL_STATE_COUNT = 7    //此状态表示对等连接状态的最大数量
}

object EasyRTCIceTransportPolicy {
    const val EaSyRTC_ICE_TRANSPORT_POLICY_RELAY = 1 // ICE代理只使用媒体中继
    const val EaSyRTC_ICE_TRANSPORT_POLICY_ALL = 2 // ICE代理使用任何类型的候选
}


object EasyRTCSdk {
    private const val TAG = "EasyRTCSdk"
    private var mUserPtr: Long = 0
    private var mPeerConnection: Long = 0L
    private var video_transceiver: Long = 0L
    private var audio_transceiver: Long = 0L
    private var data_channel: Long = 0L

    private var peerConnection: peerconnection? = peerconnection()

    //TODO 线程处理
    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())

    interface EasyRTCEventListener {
        fun connectionStateChange(state: Int)
        fun onSDPCallback(isOffer: Int, sdp: String)
        fun onTransceiverCallback(track: Int, codecId: Int, frameType: Int, frameData: ByteArray, frameSize: Int, pts: Long)
        fun onDataChannelCallback(type: Int, binary: Int, data: ByteArray, size: Int)
    }

    private var eventListener: EasyRTCEventListener? = null

    fun setEventListener(listener: EasyRTCEventListener?) {
        this.eventListener = listener
    }

    fun getInstance(): peerconnection {

        if (peerConnection == null) peerConnection = peerconnection()
        return peerConnection!!
    }

    //获取audio_transceiver
    fun getAudioTransceiver(): Long {
        return this.audio_transceiver
    }

    // 提供封装的方法
    fun connection(stun: String, turn: String, username: String, credential: String, version: Int = 0, iceTransportPolicy: Int = 2, userPtr: Long = 0) {
        this.mUserPtr = userPtr
        this.mPeerConnection = peerConnection!!.create(version, iceTransportPolicy, stun, turn, username, credential, userPtr)
        Log.d(TAG, "初始化中... this.mPeerConnection = ${this.mPeerConnection}")
    }

    fun addTransceiver(codecID: Int, streamId: String, trackId: String, streamTrackType: Int, direction: Int) {
        val transceiver = peerConnection!!.AddTransceiver(this.mPeerConnection, codecID, streamId, trackId, streamTrackType, direction, this.mUserPtr)
        if (streamTrackType == EasyRTCStreamTrack.VIDEO) {
            video_transceiver = transceiver
        } else {
            audio_transceiver = transceiver
        }

    }

    fun addDataChannel(name: String = "") {
        this.data_channel = peerConnection?.AddDataChannel(this.mPeerConnection, name, this.mUserPtr)!!
//        Log.d(TAG, "创建 createDataChannel ... data_channel = ${this.data_channel}")
    }

    fun createAnswer(sdp: String) {
//        Log.d(TAG, "创建 CreateAnswer ...")
        peerConnection?.CreateAnswer(this.mPeerConnection, sdp, this.mUserPtr)
    }

    fun getIceCandidateType() : Int {
       return peerConnection!!.GetIceCandidateType(this.mPeerConnection)
    }

    fun createOffer() {
        Log.d(TAG, "创建 createOffer ...")
        peerConnection?.CreateOffer(this.mPeerConnection, this.mUserPtr)
    }

    fun setRemoteDescription(sdp: String) {
        Log.d(TAG, "创建 SetRemoteDescription ...")
        peerConnection?.SetRemoteDescription(this.mPeerConnection, sdp)
    }

    fun sendMsg(isBinary: Int, data: ByteArray = ByteArray(0)) {
        if (this.data_channel == 0L) return
        peerConnection?.DataChannelSend(this.data_channel!!, isBinary, data, data.size)
//        Log.d(TAG, "发送数据 :data_channel = ${this.data_channel},isBinary= $isBinary,msg= $msg size = ${data.size}")
    }

    fun sendVideoFrame(data: ByteArray, keyframe: Int, pts: Long) {
        if (this.video_transceiver == 0L) return
//        Log.d(TAG, "sendVideoFrame.....= ${this.video_transceiver} $keyframe")
        peerConnection?.SendVideoFrame(this.video_transceiver, data, data.size, keyframe, pts)
    }

    fun sendAudioFrame(data: ByteArray, pts: Long) {
        if (this.audio_transceiver == 0L) return
//        Log.d(TAG, "sendAudioFrame.....= ${this.audio_transceiver}")
        peerConnection?.SendAudioFrame(this.audio_transceiver, data, data.size, pts)
    }


    fun release() {
        if (this.video_transceiver != 0L) peerConnection?.FreeTransceiver(this.video_transceiver)
        if (this.audio_transceiver != 0L) peerConnection?.FreeTransceiver(this.audio_transceiver)
        if (this.data_channel != 0L) peerConnection?.FreeDataChannel(this.data_channel)

        if (this.mPeerConnection != 0L) peerConnection?.release(this.mPeerConnection)

        this.mPeerConnection = 0L
        this.video_transceiver = 0L
        this.audio_transceiver = 0L
        this.data_channel = 0L
    }


    @JvmStatic
    fun connectionStateChange(userPtr: Long, state: Int): Int {
        if (userPtr == this.mUserPtr) eventListener?.connectionStateChange(state)
        return 0
    }

    @JvmStatic
    fun onSDPCallback(userPtr: Long, isOffer: Int, sdp: String): Int {
        executor.execute {
            mainHandler.post {
                if (userPtr == this.mUserPtr) eventListener?.onSDPCallback(isOffer, sdp)
            }
        }
        return 0
    }

    @JvmStatic
    fun onTransceiverCallback(userPtr: Long, type: Int, codecId: Int, frameType: Int, frameData: ByteArray, frameSize: Int, pts: Long, bandwidthEstimation: Int): Int {
        executor.execute {
            mainHandler.post {
                if (userPtr == this.mUserPtr) eventListener?.onTransceiverCallback(type, codecId, frameType, frameData, frameSize, pts)
            }
        }
        return 0
    }

    @JvmStatic
    fun onDataChannelCallback(userPtr: Long, type: Int, binary: Int, data: ByteArray, size: Int): Int {
        executor.execute {
            mainHandler.post {
                eventListener?.onDataChannelCallback(type, binary, data, size)
            }
        }
        return 0
    }


}