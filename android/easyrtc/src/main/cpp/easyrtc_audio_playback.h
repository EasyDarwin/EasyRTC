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
#include "easyrtc_audio_decoder.h"
#include "utils/ringbuffer.hpp"
#include "sonic.h"

struct AudioPlaybackPipeline {
    AAudioStream* stream = nullptr;
    easyrtc::vector_rb jitterBuffer{MAX_QUEUE_SIZE};
    std::mutex queueMutex;
    std::condition_variable queueCv;
    std::atomic<bool> playing{false};
    std::atomic<bool> stopped{false};

    static const size_t MIN_QUEUE_SIZE = 4;
    static const size_t MAX_QUEUE_SIZE = 200;
    static const size_t SPEED_UP_THRESHOLD = 20;
    int32_t SAMPLE_RATE = 8000;
    int32_t CHANNEL_COUNT = 1;
    size_t FRAME_SIZE = 320;

    AudioPlaybackPipeline() = default;
    ~AudioPlaybackPipeline() = default;
    AudioPlaybackPipeline(const AudioPlaybackPipeline&) = delete;
    AudioPlaybackPipeline& operator=(const AudioPlaybackPipeline&) = delete;


    AudioDecoderPipeline* audioDecoder{nullptr};
    std::shared_ptr<sonicStreamStruct> sonicStream{nullptr};

    std::vector<uint8_t> remaining_pcm_;
    bool lack_of_pcm_ = false;
};

std::shared_ptr<AudioPlaybackPipeline> audioPlaybackCreate(int audioCodec);
void audioPlaybackEnqueueFrame(std::shared_ptr<AudioPlaybackPipeline> pipeline, const uint8_t* data, int32_t size);
void audioPlaybackRelease(std::shared_ptr<AudioPlaybackPipeline> pipeline);

#endif
