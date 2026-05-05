#ifndef EASYRTC_SESSION_H
#define EASYRTC_SESSION_H

#include "easyrtc_common.h"
#include "easyrtc_frame_dump.h"
#include <android/native_window_jni.h>
#include <android/surface_texture.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCaptureRequest.h>
#include <atomic>
#include <mutex>
#include <array>
#include <thread>
#include <string>

#include <jni.h>

#if 0
    // 100ns = 0.1us, so *10 to get microseconds
    // frame->presentationTs * 10 / 90000 => microseconds, so, presentationTs/9000=> milliseconds
    #define VIDEO_PTS_TO_US(presentationTs) (static_cast<int64_t>(presentationTs) / 9000)
    #define AUDIO_SAMPLES_TO_US(SAMPLES, sampleRate) (static_cast<int64_t>(SAMPLES) * 1000000 / (sampleRate))
#else
    // the unit should be 100us!!!
    // so, presentationTs*100/90000 => microseconds
    #define VIDEO_PTS_TO_US(presentationTs) (static_cast<int64_t>(presentationTs) / 900)
    #define AUDIO_PTS_TO_US(presentationTs, sampleRate) (static_cast<int64_t>(presentationTs) * 100 / sampleRate)
    #define AUDIO_SAMPLES_TO_US(SAMPLES, sampleRate) (static_cast<int64_t>(SAMPLES) * 1000000 / (sampleRate))
#endif
struct AudioPlaybackPipeline;
struct VideoDecoderPipeline;
struct AudioCapturePipeline;
struct MediaPipeline;
struct EncoderGlBridge;
struct MediaSession {
    EasyRTC_PeerConnection peerConnection = nullptr;
    EasyRTC_Transceiver videoTransceiver = nullptr;
    EasyRTC_Transceiver audioTransceiver = nullptr;

    ACameraDevice *cameraDevice = nullptr;
    int cameraFacing = 0;
    ANativeWindow *previewWindow = nullptr;
    ANativeWindow *cameraInputWindow = nullptr;
    ASurfaceTexture *cameraInputSurfaceTexture = nullptr;
    ACameraCaptureSession *captureSession = nullptr;
    ACaptureSessionOutputContainer *outputContainer = nullptr;
    ACaptureSessionOutput *previewOutput = nullptr;
    ACaptureSessionOutput *cameraInputOutput = nullptr;
    ACameraOutputTarget *previewTarget = nullptr;
    ACameraOutputTarget *cameraInputTarget = nullptr;
    ACaptureRequest *captureRequest = nullptr;
    std::mutex cameraMutex;

    std::shared_ptr<MediaPipeline> videoEncoder = nullptr;
    std::shared_ptr<EncoderGlBridge> encoderGlBridge = nullptr;
    std::shared_ptr<AudioCapturePipeline> audioCapture = nullptr;
    std::shared_ptr<AudioPlaybackPipeline> audioPlayback = nullptr;
    struct AudioDecoderPipeline *audioDecoder = nullptr;
    std::shared_ptr<VideoDecoderPipeline> videoDecoder = nullptr;
    ANativeWindow *decoderSurface = nullptr;

    JavaVM *jvm = nullptr;
    jobject javaObj = nullptr;
    jobject cameraInputSurfaceTextureObj = nullptr;

    std::atomic<bool> previewRunning{false};
    std::atomic<bool> transceiversAdded{false};
    int videoCodec = 1;
    int audioCodec = 5;
    int encoderRotation = 90;
    bool encoderSwapWH = true;
    std::array<float, 16> cameraInputTransform = {
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
    };
    std::atomic<bool> renderThreadRunning{false};
    std::thread renderThread;
    FrameDumpWriter frameDump;

    std::atomic<uint64_t> mediaInputVideoBytesWindow{0};
    std::atomic<uint64_t> mediaInputAudioBytesWindow{0};
    std::atomic<int64_t> mediaInputLastReportNs{0};
    std::atomic<uint32_t> mediaInputVideoKbpsX100{0};
    std::atomic<uint32_t> mediaInputAudioKbpsX100{0};
    std::atomic<uint32_t> mediaInputTotalKbpsX100{0};

    int connectState{};
    std::string deviceId;
};

#endif
