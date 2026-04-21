#ifndef EASYRTC_SESSION_H
#define EASYRTC_SESSION_H

#include "easyrtc_common.h"
#include <android/native_window_jni.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCaptureRequest.h>
#include <pthread.h>
#include <atomic>

struct MediaSession {
    EasyRTC_PeerConnection peerConnection = nullptr;
    EasyRTC_Transceiver videoTransceiver = nullptr;
    EasyRTC_Transceiver audioTransceiver = nullptr;

    ACameraDevice* cameraDevice = nullptr;
    int cameraFacing = 0;
    ANativeWindow* previewWindow = nullptr;
    ACameraCaptureSession* captureSession = nullptr;
    ACaptureSessionOutputContainer* outputContainer = nullptr;
    ACaptureSessionOutput* encoderOutput = nullptr;
    ACaptureSessionOutput* previewOutput = nullptr;
    ACameraOutputTarget* encoderTarget = nullptr;
    ACameraOutputTarget* previewTarget = nullptr;
    ACaptureRequest* captureRequest = nullptr;
    pthread_mutex_t cameraMutex;
    MediaSession() { pthread_mutex_init(&cameraMutex, nullptr); }

    struct MediaPipeline* videoEncoder = nullptr;
    struct AudioCapturePipeline* audioCapture = nullptr;
    struct AudioPlaybackPipeline* audioPlayback = nullptr;
    struct VideoDecoderPipeline* videoDecoder = nullptr;
    ANativeWindow* decoderSurface = nullptr;

    std::atomic<bool> running{false};
    std::atomic<bool> previewRunning{false};
    std::atomic<bool> transceiversAdded{false};
    int videoCodec = 1;
    int audioCodec = 5;
};

#endif
