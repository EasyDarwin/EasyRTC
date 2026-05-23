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
    const val H265 = 7
    const val AAC = 8
    const val UNKNOWN = 9
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
    private var data_channel: Long = 0L

    private var mediaSession: MediaSession? = null
    private var pendingStun: String = ""
    private var pendingTurn: String = ""
    private var pendingTurnUser: String = ""
    private var pendingTurnPass: String = ""

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

    fun getInstance(): EasyRTCSdk {
        EasyRTCLog.d(TAG, "getInstance")
        return this
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
        this.pendingStun = stun
        this.pendingTurn = turn
        this.pendingTurnUser = username
        this.pendingTurnPass = credential
        EasyRTCLog.i(TAG, "connection: storing ICE config userPtr=$userPtr stun=$stun turn=${turn.isNotBlank()}")
    }

    fun addDataChannel(name: String = "") {
        this.data_channel = mediaSession?.addDataChannel(name) ?: 0L
        EasyRTCLog.i(TAG, "addDataChannel: name=$name handle=$data_channel")
    }

    fun createAnswer(sdp: String) {
        EasyRTCLog.i(TAG, "createAnswer: offerSdpLength=${sdp.length}")
        mediaSession?.createAnswer(sdp)
    }

    fun getIceCandidateType(): Int {
        val type = mediaSession?.getIceCandidateType() ?: -1
        EasyRTCLog.i(TAG, "getIceCandidateType: $type")
        return type
    }

    fun createOffer() {
        EasyRTCLog.i(TAG, "createOffer")
        mediaSession?.createOffer()
    }

    fun setRemoteDescription(sdp: String) {
        EasyRTCLog.i(TAG, "setRemoteDescription: sdpLength=${sdp.length}")
        mediaSession?.setRemoteDescription(sdp)
    }

    fun sendMsg(isBinary: Int, data: ByteArray = ByteArray(0)) {
        if (this.data_channel == 0L) return
        EasyRTCLog.d(TAG, "sendMsg: isBinary=$isBinary size=${data.size} channel=$data_channel")
        mediaSession?.dataChannelSend(this.data_channel, isBinary != 0, data)
    }

    fun bindMediaSession(session: MediaSession) {
        this.mediaSession = session
        val pc = session.createPeerConnection(pendingStun, pendingTurn, pendingTurnUser, pendingTurnPass)
        EasyRTCLog.i(TAG, "bindMediaSession: session=$session pc=$pc")
    }

    fun release() {
        EasyRTCLog.i(TAG, "release: dataChannel=$data_channel")
        if (this.data_channel != 0L) mediaSession?.freeDataChannel(this.data_channel)
        mediaSession?.releasePeerConnection()
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
