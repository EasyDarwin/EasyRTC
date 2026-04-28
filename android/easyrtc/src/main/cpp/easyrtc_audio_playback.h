#ifndef EASYRTC_AUDIO_PLAYBACK_H
#define EASYRTC_AUDIO_PLAYBACK_H

#include "easyrtc_common.h"
#include <aaudio/AAudio.h>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <vector>
#include "easyrtc_audio_decoder.h"
#include "sonic.h"

struct AudioPlaybackPipeline {
    AAudioStream* stream = nullptr;
    std::atomic<bool> playing{false};
    std::atomic<bool> stopped{false};

    static constexpr int32_t SAMPLE_RATE = 8000;
    static constexpr int32_t CHANNEL_COUNT = 1;
    static constexpr float SPEED_UP_THRESHOLD_MS = 200.0f;
    static constexpr float MAX_SPEED = 2.0f;

    AudioPlaybackPipeline() = default;
    ~AudioPlaybackPipeline() = default;
    AudioPlaybackPipeline(const AudioPlaybackPipeline&) = delete;
    AudioPlaybackPipeline& operator=(const AudioPlaybackPipeline&) = delete;

    AudioDecoderPipeline* audioDecoder{nullptr};
    std::shared_ptr<sonicStreamStruct> sonicStream{nullptr};

    // std::vector<uint8_t> remaining_pcm_;
    bool lack_of_pcm_ = false;
    float currentSpeed = 1.0f;
    std::mutex mutex_;
};

std::shared_ptr<AudioPlaybackPipeline> audioPlaybackCreate(int audioCodec);
void audioPlaybackEnqueueFrame(std::shared_ptr<AudioPlaybackPipeline> pipeline, const uint8_t* data, int32_t size);
void audioPlaybackRelease(std::shared_ptr<AudioPlaybackPipeline> pipeline);

#endif