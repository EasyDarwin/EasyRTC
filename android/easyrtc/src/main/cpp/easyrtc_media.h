#ifndef EASYRTC_MEDIA_H
#define EASYRTC_MEDIA_H

#include "easyrtc_common.h"
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadataTags.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCaptureRequest.h>
#include <android/native_window_jni.h>
#include <pthread.h>
#include <atomic>
#include <cstdint>
#include <string>

struct MediaPipeline {
    AMediaCodec* encoder = nullptr;
    AMediaFormat* format = nullptr;
    ANativeWindow* window = nullptr;
    EasyRTC_Transceiver transceiver = nullptr;
    int width = 0;
    int height = 0;
    int bitrate = 0;
    int fps = 0;
    int iframeInterval = 0;
    std::atomic<bool> running{false};
    pthread_t output_thread = 0;
    uint8_t* sps_pps_buffer = nullptr;
    size_t sps_pps_size = 0;
    pthread_mutex_t mutex;

    ACameraDevice* cameraDevice = nullptr;
    ACameraCaptureSession* captureSession = nullptr;
    ACaptureSessionOutputContainer* outputContainer = nullptr;
    ACaptureSessionOutput* encoderOutput = nullptr;
    ACaptureSessionOutput* previewOutput = nullptr;
    ACameraOutputTarget* encoderTarget = nullptr;
    ACameraOutputTarget* previewTarget = nullptr;
    ACaptureRequest* captureRequest = nullptr;
    ANativeWindow* previewWindow = nullptr;
    int cameraFacing = 0;

    MediaPipeline() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    MediaPipeline(const MediaPipeline&) = delete;
    MediaPipeline& operator=(const MediaPipeline&) = delete;
    MediaPipeline(MediaPipeline&&) = delete;
    MediaPipeline& operator=(MediaPipeline&&) = delete;
};

std::string findCameraId(int facing);
bool startCameraCapture(MediaPipeline* pipeline);
void releaseCaptureSession(MediaPipeline* pipeline);
void closeCamera(MediaPipeline* pipeline);
void* outputThreadFunc(void* arg);

#endif
