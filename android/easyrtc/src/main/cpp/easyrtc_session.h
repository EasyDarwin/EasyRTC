#ifndef EASYRTC_SESSION_H
#define EASYRTC_SESSION_H

#include "easyrtc_common.h"
#include <android/native_window_jni.h>
#include <atomic>

struct MediaSession {
    EasyRTC_PeerConnection peerConnection = nullptr;
    EasyRTC_Transceiver videoTransceiver = nullptr;
    EasyRTC_Transceiver audioTransceiver = nullptr;

    struct MediaPipeline* videoEncoder = nullptr;
    struct AudioCapturePipeline* audioCapture = nullptr;
    struct AudioPlaybackPipeline* audioPlayback = nullptr;
    struct VideoDecoderPipeline* videoDecoder = nullptr;

    ANativeWindow* previewWindow = nullptr;
    ANativeWindow* decoderSurface = nullptr;

    std::atomic<bool> running{false};
    std::atomic<bool> transceiversAdded{false};
    int videoCodec = 1;
    int audioCodec = 5;
};

#endif
