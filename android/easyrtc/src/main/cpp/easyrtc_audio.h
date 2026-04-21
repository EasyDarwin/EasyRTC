#ifndef EASYRTC_AUDIO_H
#define EASYRTC_AUDIO_H

#include "easyrtc_common.h"
#include <aaudio/AAudio.h>
#include <atomic>
#include <cstdint>
#include <mutex>

struct AudioCapturePipeline {
    AAudioStream* stream = nullptr;
    EasyRTC_Transceiver audioTransceiver = nullptr;
    std::atomic<bool> running{false};
    std::mutex mutex;

    AudioCapturePipeline() = default;
    ~AudioCapturePipeline() = default;
    AudioCapturePipeline(const AudioCapturePipeline&) = delete;
    AudioCapturePipeline& operator=(const AudioCapturePipeline&) = delete;
};

AudioCapturePipeline* audioCaptureCreate(EasyRTC_Transceiver audioTransceiver);
int audioCaptureStart(AudioCapturePipeline* pipeline);
void audioCaptureStop(AudioCapturePipeline* pipeline);
void audioCaptureRelease(AudioCapturePipeline* pipeline);

#endif
