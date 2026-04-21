#ifndef EASYRTC_AUDIO_PLAYBACK_H
#define EASYRTC_AUDIO_PLAYBACK_H

#include "easyrtc_common.h"
#include <aaudio/AAudio.h>
#include <pthread.h>
#include <atomic>
#include <cstdint>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>

struct AudioPlaybackPipeline {
    AAudioStream* stream = nullptr;
    std::queue<std::vector<uint8_t>> jitterBuffer;
    std::mutex queueMutex;
    std::condition_variable queueCv;
    std::atomic<bool> playing{false};
    std::atomic<bool> stopped{false};

    static constexpr size_t MAX_QUEUE_SIZE = 200;
    static constexpr int32_t SAMPLE_RATE = 8000;
    static constexpr int32_t CHANNEL_COUNT = 1;
    static constexpr size_t FRAME_SIZE = 320;

    AudioPlaybackPipeline() = default;
    ~AudioPlaybackPipeline() = default;
    AudioPlaybackPipeline(const AudioPlaybackPipeline&) = delete;
    AudioPlaybackPipeline& operator=(const AudioPlaybackPipeline&) = delete;
};

AudioPlaybackPipeline* audioPlaybackCreate();
void audioPlaybackEnqueueFrame(AudioPlaybackPipeline* pipeline, const uint8_t* data, int32_t size);
void audioPlaybackRelease(AudioPlaybackPipeline* pipeline);

#endif
