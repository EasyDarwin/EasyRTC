#ifndef EASYRTC_VIDEO_DECODER_H
#define EASYRTC_VIDEO_DECODER_H

#include "easyrtc_common.h"
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <android/native_window_jni.h>
#include <atomic>
#include <cstdint>
#include <queue>
#include <vector>
#include <mutex>
#include <string>
#include <thread>

struct VideoDecoderPipeline {
    AMediaCodec* decoder = nullptr;
    ANativeWindow* surface = nullptr;
    std::queue<std::vector<uint8_t>> frameQueue;
    std::mutex queueMutex;
    std::mutex decoderMutex;
    std::atomic<bool> running{false};
    std::atomic<bool> destroyed{false};
    std::thread decodeThread;
    std::string currentCodecType;
    int width = 0;
    int height = 0;
    int frameRate = 30;
    std::atomic<int> errorCount{0};

    static constexpr int MAX_ERROR_COUNT = 5;
    static constexpr size_t MAX_QUEUE_SIZE = 30;
    static constexpr int64_t DEQUEUE_TIMEOUT_US = 10000;

    VideoDecoderPipeline() = default;
    ~VideoDecoderPipeline() = default;
    VideoDecoderPipeline(const VideoDecoderPipeline&) = delete;
    VideoDecoderPipeline& operator=(const VideoDecoderPipeline&) = delete;
};

VideoDecoderPipeline* videoDecoderCreate(ANativeWindow* surface, int codecType, int width, int height);
int videoDecoderStart(VideoDecoderPipeline* pipeline);
void videoDecoderEnqueueFrame(VideoDecoderPipeline* pipeline, const uint8_t* data, int32_t size);
void videoDecoderReinit(VideoDecoderPipeline* pipeline, int codecType);
void videoDecoderRelease(VideoDecoderPipeline* pipeline);

#endif
