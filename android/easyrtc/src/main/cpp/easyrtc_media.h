#ifndef EASYRTC_MEDIA_H
#define EASYRTC_MEDIA_H

#include "easyrtc_common.h"
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <mutex>

struct MediaPipeline {
    AMediaCodec* encoder = nullptr;
    std::shared_ptr<AMediaFormat> format = nullptr;
    std::shared_ptr<ANativeWindow> encoderInputSurface = nullptr;
    EasyRTC_Transceiver transceiver = nullptr;
    int width = 0;
    int height = 0;
    int bitrate = 0;
    int fps = 0;
    int iframeInterval = 0;
    std::atomic<bool> running{false};
    std::thread outputThread;
    std::vector<uint8_t> sps_pps_buffer;
    int sps_pps_size{0};
    std::recursive_mutex mutex;
    std::string mime;

    MediaPipeline() = default;
    MediaPipeline(const MediaPipeline&) = delete;
    MediaPipeline& operator=(const MediaPipeline&) = delete;
    MediaPipeline(MediaPipeline&&) = delete;
    MediaPipeline& operator=(MediaPipeline&&) = delete;


    bool initEncoder();
};

std::string findCameraId(int facing);
void* outputThreadFunc(void* arg);

#endif
