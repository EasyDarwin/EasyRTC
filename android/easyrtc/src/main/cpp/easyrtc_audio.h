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

std::shared_ptr<AudioCapturePipeline> audioCaptureCreate(EasyRTC_Transceiver audioTransceiver);
int audioCaptureStart(std::shared_ptr<AudioCapturePipeline> pipeline);
void audioCaptureStop(std::shared_ptr<AudioCapturePipeline> pipeline);
void audioCaptureRelease(std::shared_ptr<AudioCapturePipeline> pipeline);

#endif
