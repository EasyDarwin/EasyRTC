package cn.easyrtc

import android.os.Handler
import android.os.Looper
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

    private val executor: ExecutorService = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())

    interface EasyRTCEventListener {
        fun connectionStateChange(state: Int)
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

    @JvmStatic
    fun notifyRemoteVideoSize(width: Int, height: Int) {
        EasyRTCLog.d(TAG, "notifyRemoteVideoSize: ${width}x$height")
        mainHandler.post {
            eventListener?.onRemoteVideoSize(width, height)
        }
    }

    // ─── Legacy JNI dispatch (kept for peerconnection.java / libEasyRTC.so compatibility) ───

    @Keep
    @JvmStatic
    fun connectionStateChange(userPtr: Long, state: Int): Int {
        EasyRTCLog.i(TAG, "connectionStateChange: userPtr=$userPtr state=$state")
        eventListener?.connectionStateChange(state)
        return 0
    }

    @Keep
    @JvmStatic
    fun onSDPCallback(userPtr: Long, isOffer: Int, sdp: String): Int {
        EasyRTCLog.w(TAG, "onSDPCallback called via legacy path — SDP should go through MediaSession lambdas")
        return 0
    }

    @Keep
    @JvmStatic
    fun onTransceiverCallback(userPtr: Long, type: Int, codecId: Int, frameType: Int, frameData: ByteArray, frameSize: Int, pts: Long, bandwidthEstimation: Int): Int {
        // Frames processed entirely in native mediaTransceiverCallback now
        return 0
    }

    @Keep
    @JvmStatic
    fun onDataChannelCallback(userPtr: Long, type: Int, binary: Int, data: ByteArray, size: Int): Int {
        EasyRTCLog.d(TAG, "onDataChannelCallback: userPtr=$userPtr type=$type")
        executor.execute {
            mainHandler.post {
                eventListener?.onDataChannelCallback(type, binary, data, size)
            }
        }
        return 0
    }
}
