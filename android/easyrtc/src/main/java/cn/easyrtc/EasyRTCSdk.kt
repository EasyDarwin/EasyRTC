package cn.easyrtc

import androidx.annotation.Keep

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

// ─── Legacy JNI dispatch stubs (kept for peerconnection.java / libEasyRTC.so) ───

object EasyRTCSdk {
    @Keep @JvmStatic fun connectionStateChange(userPtr: Long, state: Int): Int = 0
    @Keep @JvmStatic fun onSDPCallback(userPtr: Long, isOffer: Int, sdp: String): Int = 0
    @Keep @JvmStatic fun onTransceiverCallback(userPtr: Long, type: Int, codecId: Int, frameType: Int, frameData: ByteArray, frameSize: Int, pts: Long, bandwidthEstimation: Int): Int = 0
    @Keep @JvmStatic fun onDataChannelCallback(userPtr: Long, type: Int, binary: Int, data: ByteArray, size: Int): Int = 0
    @Keep @JvmStatic fun notifyRemoteVideoSize(width: Int, height: Int) {}
}