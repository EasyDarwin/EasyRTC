package cn.easyrtc;

/**
 * Stub retained for libEasyRTC.so JNI class resolution.
 * All peer connection operations now go through MediaSession native code.
 */
public class peerconnection {

    // Static callbacks — method IDs are resolved by libEasyRTC.so on load.
    // No longer the active callback path (native C callbacks are used instead),
    // but must exist for libEasyRTC.so's JNI_OnLoad.
    public static int OnEasyRTC_ConnectionStateChange_Callback(long userPtr, int state) {
        return 0;
    }

    public static int OnEasyRTC_Transceiver_Callback(long userPtr, int type, int codecId, int frameType, byte[] frameData, int frameSize, long pts, int bandwidthEstimation) {
        return 0;
    }

    public static int OnEasyRTC_IceCandidate_Callback(long userPtr, int isOffer, String sdp) {
        return 0;
    }

    public static int OnEasyRTC_DataChannel_Callback(long userPtr, int type, int binary, byte[] jbytearray, int size) {
        EasyRTCSdk.onDataChannelCallback(userPtr, type, binary, jbytearray, size);
        return 0;
    }
}
