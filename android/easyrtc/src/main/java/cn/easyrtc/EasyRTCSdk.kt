package cn.easyrtc

import android.os.Handler
import android.os.Looper
import cn.easyrtc.media.MediaSession
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

    fun setEventListener(listener: EasyRTCEventListener) {
        this.eventListener = listener
        EasyRTCLog.i(TAG, "setEventListener: listener=${listener?.javaClass?.simpleName ?: "null"}")
    }

    fun unsetEventListener(listener: EasyRTCEventListener) {
        if (listener == this.eventListener) {
            this.eventListener = null;
            EasyRTCLog.i(TAG, "unsetEventListener: listener=${listener?.javaClass?.simpleName ?: "null"}")
        } else {
            EasyRTCLog.w(TAG, "unsetEventListener: listener=${listener?.javaClass?.simpleName ?: "null"}, not equal to local!!!")
        }

    }

    fun getInstance(): peerconnection {
        if (peerConnection == null) peerConnection = peerconnection()
        EasyRTCLog.d(TAG, "getInstance: peerConnection=${peerConnection != null}")
        return peerConnection!!
    }
    
    @JvmStatic
    fun notifyRemoteVideoSize(width: Int, height: Int) {
        EasyRTCLog.d(TAG, "notifyRemoteVideoSize: ${width}x$height")
        mainHandler.post {
            eventListener?.onRemoteVideoSize(width, height)
        }
    }


    fun connection(stun: String, turn: String, username: String, credential: String, version: Int = 0, iceTransportPolicy: Int = 2, userPtr: Long = 0) {
        this.mUserPtr = userPtr
        this.mPeerConnection = peerConnection!!.create(version, iceTransportPolicy, stun, turn, username, credential, userPtr)
        EasyRTCLog.i(
            TAG,
            "connection: peer=$mPeerConnection userPtr=$mUserPtr version=$version policy=$iceTransportPolicy stun=$stun turn=${turn.isNotBlank()}"
        )
    }

    fun addDataChannel(name: String = "") {
        this.data_channel = peerConnection?.AddDataChannel(this.mPeerConnection, name, this.mUserPtr)!!
        EasyRTCLog.i(TAG, "addDataChannel: name=$name handle=$data_channel")
    }

    fun addTransceiver(codecId: Int, streamId: String, trackId: String, streamTrackType: Int, direction: Int) {
        val handle = peerConnection?.AddTransceiver(this.mPeerConnection, codecId, streamId, trackId, streamTrackType, direction, this.mUserPtr)
        EasyRTCLog.i(
            TAG,
            "addTransceiver: codec=$codecId stream=$streamId track=$trackId type=$streamTrackType direction=$direction handle=$handle"
        )
    }

    fun createAnswer(sdp: String) {
        EasyRTCLog.i(TAG, "createAnswer: offerSdpLength=${sdp.length}")
        peerConnection?.CreateAnswer(this.mPeerConnection, sdp, this.mUserPtr)
    }

    fun getIceCandidateType() : Int {
       val type = peerConnection!!.GetIceCandidateType(this.mPeerConnection)
       EasyRTCLog.i(TAG, "getIceCandidateType: $type")
       return type
    }

    fun createOffer() {
        EasyRTCLog.i(TAG, "createOffer: peer=$mPeerConnection userPtr=$mUserPtr")
        peerConnection?.CreateOffer(this.mPeerConnection, this.mUserPtr)
    }

    fun setRemoteDescription(sdp: String) {
        EasyRTCLog.i(TAG, "setRemoteDescription: sdpLength=${sdp.length}")
        peerConnection?.SetRemoteDescription(this.mPeerConnection, sdp)
    }

    fun sendMsg(isBinary: Int, data: ByteArray = ByteArray(0)) {
        if (this.data_channel == 0L) return
        EasyRTCLog.d(TAG, "sendMsg: isBinary=$isBinary size=${data.size} channel=$data_channel")
        peerConnection?.DataChannelSend(this.data_channel!!, isBinary, data, data.size)
    }

    fun bindMediaSession(session:MediaSession) {
        session.setPeerConnection(mPeerConnection)
        EasyRTCLog.i(TAG, "bindMediaSession: peer=$mPeerConnection")
    }

    fun release() {
        EasyRTCLog.i(TAG, "release: peer=$mPeerConnection dataChannel=$data_channel")

        if (this.data_channel != 0L) peerConnection?.FreeDataChannel(this.data_channel)
        if (this.mPeerConnection != 0L) peerConnection?.release(this.mPeerConnection)

        this.mPeerConnection = 0L
        this.data_channel = 0L
    }


    @Keep
    @JvmStatic
    fun connectionStateChange(userPtr: Long, state: Int): Int {
        EasyRTCLog.i(TAG, "connectionStateChange: userPtr=$userPtr state=$state match=${userPtr == this.mUserPtr}")
        if (userPtr == this.mUserPtr) eventListener?.connectionStateChange(state)

        return 0
    }

    @Keep
    @JvmStatic
    fun onSDPCallback(userPtr: Long, isOffer: Int, sdp: String): Int {
        EasyRTCLog.i(TAG, "onSDPCallback: userPtr=$userPtr isOffer=$isOffer sdpLength=${sdp.length}")
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
        EasyRTCLog.d(
            TAG,
            "onTransceiverCallback: userPtr=$userPtr type=$type codec=$codecId frameType=$frameType frameSize=$frameSize pts=$pts bwe=$bandwidthEstimation"
        )
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
        EasyRTCLog.d(TAG, "onDataChannelCallback: userPtr=$userPtr type=$type binary=$binary size=$size")
        executor.execute {
            mainHandler.post {
                eventListener?.onDataChannelCallback(type, binary, data, size)
            }
        }
        return 0
    }
}
