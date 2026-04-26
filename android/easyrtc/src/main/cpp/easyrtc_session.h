#ifndef EASYRTC_SESSION_H
#define EASYRTC_SESSION_H

#include "easyrtc_common.h"
#include "easyrtc_frame_dump.h"
#include <android/native_window_jni.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCaptureRequest.h>
#include <atomic>
#include <mutex>

#include <jni.h>

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
    std::mutex cameraMutex;

    struct MediaPipeline* videoEncoder = nullptr;
    struct AudioCapturePipeline* audioCapture = nullptr;
    struct AudioPlaybackPipeline* audioPlayback = nullptr;
    struct AudioDecoderPipeline* audioDecoder = nullptr;
    struct VideoDecoderPipeline* videoDecoder = nullptr;
    ANativeWindow* decoderSurface = nullptr;

    JavaVM* jvm = nullptr;
    jobject javaObj = nullptr;

    std::atomic<bool> running{false};
    std::atomic<bool> previewRunning{false};
    std::atomic<bool> transceiversAdded{false};
    int videoCodec = 1;
    int audioCodec = 5;
    FrameDumpWriter frameDump;
};

#endif
