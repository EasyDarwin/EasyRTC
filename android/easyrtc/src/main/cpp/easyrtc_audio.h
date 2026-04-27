#ifndef EASYRTC_AUDIO_H
#define EASYRTC_AUDIO_H

#include "easyrtc_common.h"
#include <aaudio/AAudio.h>
#include <atomic>
#include <cstdint>
#include <mutex>

struct MediaSession;
struct AudioCapturePipeline {
    AAudioStream* stream = nullptr;
    EasyRTC_Transceiver audioTransceiver = nullptr;
    std::atomic<bool> running{false};
    std::mutex mutex;
    bool connectionReady = false;

    AudioCapturePipeline() = default;
    ~AudioCapturePipeline() = default;
    AudioCapturePipeline(const AudioCapturePipeline&) = delete;
    AudioCapturePipeline& operator=(const AudioCapturePipeline&) = delete;
};

int audioCaptureStart(MediaSession* session);
void audioCaptureStop(MediaSession* session);

#endif
