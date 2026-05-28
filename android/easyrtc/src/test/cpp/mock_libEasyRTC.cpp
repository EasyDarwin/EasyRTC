#include "EasyRTCAPI.h"
#include <android/log.h>
#include <cstdlib>
#include <cstring>
#include <chrono>

#define MOCK_LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MockEasyRTC", __VA_ARGS__)
#define MOCK_LOGW(...) __android_log_print(ANDROID_LOG_WARN, "MockEasyRTC", __VA_ARGS__)

static EasyRTC_ConnectionStateChange_Callback g_stateCallback = nullptr;
static void* g_stateUserPtr = nullptr;
static int g_sendFrameCount = 0;
static std::chrono::steady_clock::time_point g_lastVideoLog;
static std::chrono::steady_clock::time_point g_lastAudioLog;

static char g_mockSdp[] =
    "v=0\r\n"
    "o=- 123456 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
    "a=rtpmap:96 VP8/90000\r\n"
    "a=sendrecv\r\n";

static const char* transceiverCallbackTypeName(int type) {
    switch (type) {
        case 0x01: return "VIDEO_FRAME";
        case 0x02: return "AUDIO_FRAME";
        case 0x03: return "BANDWIDTH";
        case 0x04: return "KEY_FRAME_REQ";
        default: return "UNKNOWN";
    }
}

static const char* connectionStateName(int state) {
    switch (state) {
        case 0: return "NONE";
        case 1: return "NEW";
        case 2: return "CONNECTING";
        case 3: return "CONNECTED";
        case 4: return "DISCONNECTED";
        case 5: return "FAILED";
        case 6: return "CLOSED";
        default: return "UNKNOWN";
    }
}

static const char* codecName(int codec) {
    switch (codec) {
        case 1: return "H264";
        case 2: return "OPUS";
        case 3: return "VP8";
        case 4: return "MULAW";
        case 5: return "ALAW";
        case 7: return "H265";
        case 8: return "AAC";
        case 9: return "UNKNOWN";
        default: return "INVALID";
    }
}

#ifdef __cplusplus
extern "C" {
#endif

int EasyRTC_Init() {
    MOCK_LOGI("Init()");
    return 0;
}

int EasyRTC_Deinit() {
    MOCK_LOGI("Deinit()");
    return 0;
}

int EasyRTC_CreatePeerConnection(EasyRTC_PeerConnection* ppPeerConnection,
                                  EasyRTC_Configuration* pRtcConfiguration,
                                  EasyRTC_ConnectionStateChange_Callback callback,
                                  void* userptr) {
    MOCK_LOGI("CreatePeerConnection(pc=%p, config=%p, cb=%p, user=%p)",
              ppPeerConnection, pRtcConfiguration, callback, userptr);
    if (!ppPeerConnection) {
        MOCK_LOGW("CreatePeerConnection: null ppPeerConnection");
        return -1;
    }
    *ppPeerConnection = calloc(1, 64);
    g_stateCallback = callback;
    g_stateUserPtr = userptr;

    if (pRtcConfiguration) {
        MOCK_LOGI("  iceTransportPolicy=%d", pRtcConfiguration->iceTransportPolicy);
        MOCK_LOGI("  iceServers[0].urls=%s", pRtcConfiguration->iceServers[0].urls);
        MOCK_LOGI("  iceServers[1].urls=%s", pRtcConfiguration->iceServers[1].urls);
    }

    if (callback) {
        MOCK_LOGI("  -> state callback: %s", connectionStateName(EASYRTC_PEER_CONNECTION_STATE_NEW));
        callback(userptr, EASYRTC_PEER_CONNECTION_STATE_NEW);
    }

    MOCK_LOGI("CreatePeerConnection: *pc=%p", *ppPeerConnection);
    return 0;
}

int EasyRTC_ReleasePeerConnection(EasyRTC_PeerConnection* ppPeerConnection) {
    MOCK_LOGI("ReleasePeerConnection(pc=%p *pc=%p)", ppPeerConnection,
              ppPeerConnection ? *ppPeerConnection : nullptr);
    if (!ppPeerConnection || !*ppPeerConnection) {
        MOCK_LOGW("ReleasePeerConnection: null pointer");
        return -1;
    }
    free(*ppPeerConnection);
    *ppPeerConnection = nullptr;
    g_stateCallback = nullptr;
    g_stateUserPtr = nullptr;
    g_sendFrameCount = 0;
    g_lastVideoLog = std::chrono::steady_clock::time_point{};
    g_lastAudioLog = std::chrono::steady_clock::time_point{};
    MOCK_LOGI("ReleasePeerConnection: done");
    return 0;
}

int EasyRTC_AddTransceiver(EasyRTC_Transceiver* ppTransceiver,
                            EasyRTC_PeerConnection pPeerConnection,
                            EasyRTC_MediaStreamTrack* pTrack,
                            EasyRTC_RtpTransceiverInit* pInit,
                            EasyRTC_Transceiver_Callback callback,
                            void* userptr) {
    MOCK_LOGI("AddTransceiver(t=%p, pc=%p, track=%p, init=%p, cb=%p, user=%p)",
              ppTransceiver, pPeerConnection, pTrack, pInit, callback, userptr);
    if (!ppTransceiver) {
        MOCK_LOGW("AddTransceiver: null ppTransceiver");
        return -1;
    }
    *ppTransceiver = calloc(1, 64);
    if (pTrack) {
        MOCK_LOGI("  track: codec=%s(%d) kind=%d streamId=%s trackId=%s",
                  codecName(pTrack->codec), pTrack->codec, pTrack->kind,
                  pTrack->streamId, pTrack->trackId);
    }
    if (pInit) {
        MOCK_LOGI("  init: direction=%d", pInit->direction);
    }
    MOCK_LOGI("AddTransceiver: *t=%p", *ppTransceiver);

    if (g_stateCallback && g_stateUserPtr) {
        MOCK_LOGI("  -> state callback: %s", connectionStateName(EASYRTC_PEER_CONNECTION_STATE_CONNECTING));
        g_stateCallback(g_stateUserPtr, EASYRTC_PEER_CONNECTION_STATE_CONNECTING);
        MOCK_LOGI("  -> state callback: %s", connectionStateName(EASYRTC_PEER_CONNECTION_STATE_CONNECTED));
        g_stateCallback(g_stateUserPtr, EASYRTC_PEER_CONNECTION_STATE_CONNECTED);
    }
    g_sendFrameCount = 0;
    return 0;
}

int EasyRTC_SendFrame(EasyRTC_Transceiver pTransceiver, EasyRTC_Frame* pFrame) {
    if (!pFrame) {
        MOCK_LOGW("SendFrame[%d]: NULL frame!", g_sendFrameCount + 1);
        return 0;
    }
    g_sendFrameCount++;

    auto now = std::chrono::steady_clock::now();
    bool isKey = (pFrame->flags & 0x01) != 0;

    if (pFrame->trackId == 0) {
        auto elapsed = now - g_lastVideoLog;
        if (isKey && elapsed >= std::chrono::milliseconds(500)) {
            MOCK_LOGI("[VIDEO] frame#%u size=%u KEY pts=%llu track=%llu",
                      g_sendFrameCount, pFrame->size,
                      (unsigned long long)pFrame->presentationTs,
                      (unsigned long long)pFrame->trackId);
            g_lastVideoLog = now;
        } else if (elapsed >= std::chrono::seconds(1)) {
            MOCK_LOGI("[VIDEO] frame#%u size=%u    pts=%llu track=%llu",
                      g_sendFrameCount, pFrame->size,
                      (unsigned long long)pFrame->presentationTs,
                      (unsigned long long)pFrame->trackId);
            g_lastVideoLog = now;
        }
    } else {
        auto elapsed = now - g_lastAudioLog;
        if (isKey && elapsed >= std::chrono::milliseconds(500)) {
            MOCK_LOGI("[AUDIO] frame#%u size=%u KEY pts=%llu track=%llu",
                      g_sendFrameCount, pFrame->size,
                      (unsigned long long)pFrame->presentationTs,
                      (unsigned long long)pFrame->trackId);
            g_lastAudioLog = now;
        } else if (elapsed >= std::chrono::seconds(1)) {
            MOCK_LOGI("[AUDIO] frame#%u size=%u    pts=%llu track=%llu",
                      g_sendFrameCount, pFrame->size,
                      (unsigned long long)pFrame->presentationTs,
                      (unsigned long long)pFrame->trackId);
            g_lastAudioLog = now;
        }
    }
    return 0;
}

int EasyRTC_FreeTransceiver(EasyRTC_Transceiver* ppTransceiver) {
    MOCK_LOGI("FreeTransceiver(t=%p *t=%p)", ppTransceiver,
              ppTransceiver ? *ppTransceiver : nullptr);
    if (!ppTransceiver || !*ppTransceiver) {
        MOCK_LOGW("FreeTransceiver: null pointer");
        return -1;
    }
    free(*ppTransceiver);
    *ppTransceiver = nullptr;
    return 0;
}

int EasyRTC_AddDataChannel(EasyRTC_DataChannel* ppRtcDataChannel,
                            EasyRTC_PeerConnection pPeerConnection,
                            const char* pDataChannelName,
                            EasyRTC_DataChannel_Callback callback,
                            void* userptr) {
    MOCK_LOGI("AddDataChannel(dc=%p, pc=%p, name='%s', cb=%p, user=%p)",
              ppRtcDataChannel, pPeerConnection,
              pDataChannelName ? pDataChannelName : "(null)", callback, userptr);
    if (!ppRtcDataChannel) {
        MOCK_LOGW("AddDataChannel: null ppRtcDataChannel");
        return -1;
    }
    *ppRtcDataChannel = calloc(1, 64);
    MOCK_LOGI("AddDataChannel: *dc=%p", *ppRtcDataChannel);
    return 0;
}

int EasyRTC_DataChannelSend(EasyRTC_DataChannel pRtcDataChannel, BOOL isBinary,
                             const char* pMessage, int messageLen) {
    MOCK_LOGI("DataChannelSend: isBinary=%d len=%d msg='%.64s'",
              isBinary, messageLen, pMessage ? pMessage : "(null)");
    return 0;
}

int EasyRTC_FreeDataChannel(EasyRTC_DataChannel* ppRtcDataChannel) {
    MOCK_LOGI("FreeDataChannel(dc=%p *dc=%p)", ppRtcDataChannel,
              ppRtcDataChannel ? *ppRtcDataChannel : nullptr);
    if (!ppRtcDataChannel || !*ppRtcDataChannel) {
        MOCK_LOGW("FreeDataChannel: null pointer");
        return -1;
    }
    free(*ppRtcDataChannel);
    *ppRtcDataChannel = nullptr;
    return 0;
}

int EasyRTC_CreateOffer(EasyRTC_PeerConnection pPeerConnection,
                         EasyRTC_IceCandidate_Callback callback,
                         void* userptr) {
    MOCK_LOGI("CreateOffer: pc=%p cb=%p user=%p", pPeerConnection, callback, userptr);
    if (callback) {
        MOCK_LOGI("  -> invoking callback with mock SDP (isOffer=1, len=%zu)", strlen(g_mockSdp));
        callback(userptr, 1, g_mockSdp);
    }
    return 0;
}

int EasyRTC_SetRemoteDescription(EasyRTC_PeerConnection pPeerConnection,
                                  const char* sdp) {
    MOCK_LOGI("SetRemoteDescription: pc=%p sdpLen=%zu", pPeerConnection, sdp ? strlen(sdp) : 0);
    if (sdp) {
        MOCK_LOGI("  sdp='%.128s'", sdp);
    }
    return 0;
}

int EasyRTC_CreateAnswer(EasyRTC_PeerConnection pPeerConnection,
                          const char* offersdp,
                          EasyRTC_IceCandidate_Callback callback,
                          void* userptr) {
    MOCK_LOGI("CreateAnswer: pc=%p offerLen=%zu cb=%p user=%p",
              pPeerConnection, offersdp ? strlen(offersdp) : 0, callback, userptr);
    if (callback) {
        MOCK_LOGI("  -> invoking callback with mock SDP (isOffer=0, len=%zu)", strlen(g_mockSdp));
        callback(userptr, 0, g_mockSdp);
    }
    return 0;
}

int EasyRTC_GetIceCandidatePairStats(EasyRTC_PeerConnection pPeerConnection,
                                      EasyRTC_IceCandidatePairStats* pStats) {
    MOCK_LOGI("GetIceCandidatePairStats: pc=%p stats=%p", pPeerConnection, pStats);
    if (pStats) {
        memset(pStats, 0, sizeof(EasyRTC_IceCandidatePairStats));
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

int MockEasyRTC_GetSendFrameCount() {
    return g_sendFrameCount;
}

#include <jni.h>

extern "C" JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeMockGetSendFrameCount(JNIEnv*, jobject) {
    return g_sendFrameCount;
}
