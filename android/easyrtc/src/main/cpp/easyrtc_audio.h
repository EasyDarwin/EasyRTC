#ifndef EASYRTC_AUDIO_H
#define EASYRTC_AUDIO_H

#include "easyrtc_common.h"
#include <aaudio/AAudio.h>
#include <pthread.h>
#include <atomic>
#include <cstdint>

struct AudioCapturePipeline {
    AAudioStream* stream = nullptr;
    EasyRTC_Transceiver audioTransceiver = nullptr;
    std::atomic<bool> running{false};
    pthread_mutex_t mutex;

    AudioCapturePipeline() {
        pthread_mutex_init(&mutex, nullptr);
    }
    ~AudioCapturePipeline() {
        pthread_mutex_destroy(&mutex);
    }
    AudioCapturePipeline(const AudioCapturePipeline&) = delete;
    AudioCapturePipeline& operator=(const AudioCapturePipeline&) = delete;
};

AudioCapturePipeline* audioCaptureCreate(EasyRTC_Transceiver audioTransceiver);
int audioCaptureStart(AudioCapturePipeline* pipeline);
void audioCaptureStop(AudioCapturePipeline* pipeline);
void audioCaptureRelease(AudioCapturePipeline* pipeline);

#endif
