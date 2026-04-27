package cn.easyrtc

import android.os.Handler
import android.os.Looper
import android.util.Log
import cn.easyrtc.media.MediaSession
import cn.easyrtc.model.VideoEncodeConfig
import androidx.annotation.Keep
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
    const val AUDIO = 1
    const val VIDEO = 2
}


object EasyRTCDirection {
    const val EasyRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV = 1
    const val EaSyRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY = 2
    const val EaSyRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY = 3
    const val EasyRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE = 4
}

object EasyRTCPeerConnectionState {
    const val EASYRTC_PEER_CONNECTION_STATE_NONE = 0
    const val EASYRTC_PEER_CONNECTION_STATE_NEW = 1
    const val EASYRTC_PEER_CONNECTION_STATE_CONNECTING = 2
    const val EASYRTC_PEER_CONNECTION_STATE_CONNECTED = 3
    const val EASYRTC_PEER_CONNECTION_STATE_DISCONNECTED = 4
    const val EASYRTC_PEER_CONNECTION_STATE_FAILED = 5
    const val EASYRTC_PEER_CONNECTION_STATE_CLOSED = 6
    const val EASYRTC_PEER_CONNECTION_TOTAL_STATE_COUNT = 7
}

object EasyRTCIceTransportPolicy {
    const val EaSyRTC_ICE_TRANSPORT_POLICY_RELAY = 1
    const val EaSyRTC_ICE_TRANSPORT_POLICY_ALL = 2
}


object EasyRTCSdk {
    private const val TAG = "EasyRTCSdk"
    private var mUserPtr: Long = 0
    private var mPeerConnection: Long = 0L
    private var data_channel: Long = 0L

    private var peerConnection: peerconnection? = peerconnection()

    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())

    interface EasyRTCEventListener {
        fun connectionStateChange(state: Int)
        fun onSDPCallback(isOffer: Int, sdp: String)
        fun onTransceiverCallback(track: Int, codecId: Int, frameType: Int, frameData: ByteArray, frameSize: Int, pts: Long)
        fun onDataChannelCallback(type: Int, binary: Int, data: ByteArray, size: Int)
        fun onRemoteVideoSize(width: Int, height: Int) {}
    }

    private var eventListener: EasyRTCEventListener? = null

    fun setEventListener(listener: EasyRTCEventListener?) {
        this.eventListener = listener
    }

    fun getInstance(): peerconnection {
        if (peerConnection == null) peerConnection = peerconnection()
        return peerConnection!!
    }
    
    @JvmStatic
    fun notifyRemoteVideoSize(width: Int, height: Int) {
        mainHandler.post {
            eventListener?.onRemoteVideoSize(width, height)
        }
    }


    fun connection(stun: String, turn: String, username: String, credential: String, version: Int = 0, iceTransportPolicy: Int = 2, userPtr: Long = 0) {
        this.mUserPtr = userPtr
        this.mPeerConnection = peerConnection!!.create(version, iceTransportPolicy, stun, turn, username, credential, userPtr)
        Log.d(TAG, "初始化中... this.mPeerConnection = ${this.mPeerConnection}")
    }

    fun addDataChannel(name: String = "") {
        this.data_channel = peerConnection?.AddDataChannel(this.mPeerConnection, name, this.mUserPtr)!!
    }

    fun createAnswer(sdp: String) {
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
    }

    fun bindMediaSession(session:MediaSession) {
        assert(mPeerConnection != 0L);
        session.setPeerConnection(mPeerConnection)
    }

    fun release() {

        if (this.data_channel != 0L) peerConnection?.FreeDataChannel(this.data_channel)
        if (this.mPeerConnection != 0L) peerConnection?.release(this.mPeerConnection)

        this.mPeerConnection = 0L
        this.data_channel = 0L
    }


    @Keep
    @JvmStatic
    fun connectionStateChange(userPtr: Long, state: Int): Int {
        if (userPtr == this.mUserPtr) eventListener?.connectionStateChange(state)

        return 0
    }

    @Keep
    @JvmStatic
    fun onSDPCallback(userPtr: Long, isOffer: Int, sdp: String): Int {
        executor.execute {
            mainHandler.post {
                if (userPtr == this.mUserPtr) eventListener?.onSDPCallback(isOffer, sdp)
            }
        }
        return 0
    }

    @Keep
    @JvmStatic
    fun onTransceiverCallback(userPtr: Long, type: Int, codecId: Int, frameType: Int, frameData: ByteArray, frameSize: Int, pts: Long, bandwidthEstimation: Int): Int {
        executor.execute {
            mainHandler.post {
                if (userPtr == this.mUserPtr) eventListener?.onTransceiverCallback(type, codecId, frameType, frameData, frameSize, pts)
            }
        }
        return 0
    }

    @Keep
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
