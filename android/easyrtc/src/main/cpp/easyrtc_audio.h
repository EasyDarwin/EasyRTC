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

    uint8_t audioBuffer[4800]; // 160ms of 16-bit mono audio at 8000Hz
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
