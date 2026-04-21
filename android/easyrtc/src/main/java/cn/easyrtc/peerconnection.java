package cn.easyrtc;

public class peerconnection {

    static {
        System.loadLibrary("EasyRTC");
    }

    public native long create(int version, int iceTransportPolicy, String stun_server_url, String turn_server_url, String turn_username, String turn_credential, long user_ptr);

    //peer_connection  create 的返回值
    public native int release(long peer_connection);

    public native long AddDataChannel(long peer_connection, String name, long user_ptr);

    public native long AddTransceiver(long peer_connection, int codecID, String streamId, String trackId, int streamTrackType, int direction, long user_ptr);

    public native long FreeDataChannel(long data_channel);

    public native int DataChannelSend(long data_channel, int isBinary, byte[] data_binary, int size);

    public native int SendAudioFrame(long audio_transceiver, byte[] frame_audio_data, int frame_size, long pts);

    public native int FreeTransceiver(long transceiver);

    public native int CreateAnswer(long peer_connection, String offer_sdp, long user_ptr);

    // 设备端 API sdp 回调出来 先创建AddTransceiver 视频 和音频 addTransceiver  AddDataChannel  在调用CreateOffer  回调中生成
    public native int CreateOffer(long peer_connection, long user_ptr); //设备端

    //设备端  订阅  answer
    public native int SetRemoteDescription(long peer_connection, String sdp);//设备端

    public native int GetIceCandidateType(long peer_connection);

    //TODO 回调函数

    public static int OnEasyRTC_ConnectionStateChange_Callback(long userPtr, int state) {
        EasyRTCSdk.connectionStateChange(userPtr, state);
        return 0;
    }

    //bandwidthEstimation 贷款
    public static int OnEasyRTC_Transceiver_Callback(long userPtr, int type, int codecId, int frameType, byte[] frameData, int frameSize, long pts, int bandwidthEstimation) {
        EasyRTCSdk.onTransceiverCallback(userPtr, type, codecId, frameType, frameData, frameSize, pts, bandwidthEstimation);
        return 0;
    }


    public static int OnEasyRTC_IceCandidate_Callback(long userPtr, int isOffer, String sdp) {
        EasyRTCSdk.onSDPCallback(userPtr, isOffer, sdp);
        return 0;
    }

    public static int OnEasyRTC_DataChannel_Callback(long userPtr, int type, int binary, byte[] jbytearray, int size) {
        EasyRTCSdk.onDataChannelCallback(userPtr, type, binary, jbytearray, size);
        return 0;
    }
}
