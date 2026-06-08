#ifndef EASYRTC_AUDIO_PLAYBACK_H
#define EASYRTC_AUDIO_PLAYBACK_H

#include "session/common.h"
#include <aaudio/AAudio.h>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <vector>
#include "codec/audio_decoder.h"
#include "sonic.h"

struct MediaSession;

struct AudioPlaybackPipeline {
    AAudioStream* stream = nullptr;
    std::atomic<bool> playing{false};
    std::atomic<bool> stopped{false};

    static constexpr int32_t SAMPLE_RATE = 8000;
    static constexpr int32_t CHANNEL_COUNT = 1;

    AudioPlaybackPipeline() = default;
    ~AudioPlaybackPipeline() = default;
    AudioPlaybackPipeline(const AudioPlaybackPipeline&) = delete;
    AudioPlaybackPipeline& operator=(const AudioPlaybackPipeline&) = delete;

    AudioDecoderPipeline* audioDecoder{nullptr};
    std::shared_ptr<sonicStreamStruct> sonicStream{nullptr};

    bool lack_of_pcm_ = false;
    float currentSpeed = 1.0f;
    int64_t playedFrames = 0;
    std::mutex streamMutex;
    std::mutex mutex_;

    int64_t master_clock_us();
    int64_t cached_us();
};

std::shared_ptr<AudioPlaybackPipeline> audioPlaybackCreate(int audioCodec);
void audioPlaybackEnqueueFrame(MediaSession *session, const EasyRTC_Frame*);
void audioPlaybackRelease(MediaSession *session);

#endif
