#include "capture/audio_capture.h"
#include <aaudio/AAudio.h>
#include <cstring>
#include <time.h>
#include <assert.h>
#include <thread>
#include <chrono>
#include "session/common.h"
#include "session/media_session.h"
#include "g711.h"
#include "utils/defer.hpp"

static constexpr int32_t SAMPLE_RATE = 8000;
static constexpr int32_t CHANNEL_COUNT = 1;

static aaudio_data_callback_result_t dataCallback(
    AAudioStream* stream,
    void* userData,
    void* audioData,
    int32_t numFrames);
static void errorCallback(
    AAudioStream* stream,
    void* userData,
    aaudio_result_t error);

static int openAndStartCaptureStream(MediaSession* session,
                                     const std::shared_ptr<AudioCapturePipeline>& pipeline) {
    if (!pipeline || !pipeline->running.load()) {
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(pipeline->mutex);
        if (pipeline->stream) {
            return 0;
        }
    }

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || !builder) {
        LOGE("Failed to create AAudio stream builder: %d", result);
        return -1;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
    AAudioStreamBuilder_setSampleRate(builder, SAMPLE_RATE);
    AAudioStreamBuilder_setChannelCount(builder, CHANNEL_COUNT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setFramesPerDataCallback(builder, SAMPLE_RATE / 100 * 2);  // 20ms
    AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
    AAudioStreamBuilder_setDataCallback(builder, dataCallback, session);
    AAudioStreamBuilder_setErrorCallback(builder, errorCallback, session);

    AAudioStream* stream = nullptr;
    result = AAudioStreamBuilder_openStream(builder, &stream);
    if (result != AAUDIO_OK || !stream) {
        // Fall back to SHARED when EXCLUSIVE is unavailable after route/device changes.
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
        result = AAudioStreamBuilder_openStream(builder, &stream);
    }
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || !stream) {
        LOGE("Failed to open AAudio input stream: %d", result);
        return -1;
    }

    result = AAudioStream_requestStart(stream);
    if (result != AAUDIO_OK) {
        LOGE("Failed to start AAudio input stream: %d", result);
        AAudioStream_close(stream);
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(pipeline->mutex);
        if (!pipeline->running.load()) {
            AAudioStream_requestStop(stream);
            AAudioStream_close(stream);
            return -1;
        }
        if (pipeline->stream) {
            AAudioStream_requestStop(stream);
            AAudioStream_close(stream);
            return 0;
        }
        pipeline->stream = stream;
    }
    LOGI("[CRITICAL] AudioCaptureStart: SUCCESS stream=%p", stream);
    return 0;
}

static aaudio_data_callback_result_t dataCallback(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames) {
    auto session = static_cast<MediaSession*>(userData);
    auto begin_ = std::chrono::steady_clock::now();
    defer({
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - begin_).count();
        if (duration > 10) {
            LOGW("[OA] callback took %lld ms", static_cast<long long>(duration));
        }
    });
    auto* pipeline = session->audioCapture.get();
    if (session->shuttingDown || !pipeline || !pipeline->running.load()) {
        LOGW("[AUDIO] Capture callback with pipeline stopped or null, stopping capture");
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    if (!pipeline->audioTransceiver || session->connectState != 3) {
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    int32_t dataSize = numFrames * CHANNEL_COUNT * sizeof(int16_t);
    auto* pcmData = static_cast<PBYTE>(audioData);

    int64_t frameIndex = 0;
    int64_t timeNanos = 0;
    auto tsResult = AAudioStream_getTimestamp(stream, CLOCK_MONOTONIC, &frameIndex, &timeNanos);
    // EasyRTC expects 100ns ticks, so convert from CLOCK_MONOTONIC nanoseconds by /100.
    UINT64 pts100ns = 0;
    if (tsResult == AAUDIO_OK) {
        pts100ns = static_cast<UINT64>(timeNanos / 100);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        pts100ns = static_cast<UINT64>((ts.tv_sec * 1000000000LL + ts.tv_nsec) / 100);
    }
    FLOGI("[OA] PCM callback frames=%d, size=%d, pts100ns=%llu", numFrames, dataSize, static_cast<unsigned long long>(pts100ns));

#if 1
    {
        auto begin_ = std::chrono::steady_clock::now();
        defer({
                  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - begin_).count();
                  if (duration > 10) {
                      LOGW("[OA] linear2alaw took %lld ms", static_cast<long long>(duration));
                  }
        });

        // first let's convert PCM into PCMA
        for (int i = 0; i < numFrames * CHANNEL_COUNT; ++i) {
            int16_t sample = reinterpret_cast<int16_t*>(pcmData)[i];
            uint8_t encodedSample;
            if (sample >= 0) {
                encodedSample = (sample >> 8) + 128;
            } else {
                encodedSample = 256 - ((-sample) >> 8);
            }
            pipeline->audioBuffer[i] = linear2alaw(sample);
        }
    }



    EasyRTC_Frame frame{};
    frame.version = 0;
    frame.size = static_cast<UINT32>(dataSize >> 1); // PCMA is half the size of PCM
    frame.flags = static_cast<EasyRTC_FRAME_FLAGS>(EASYRTC_FRAME_FLAG_KEY_FRAME);
    frame.presentationTs = pts100ns;
    frame.decodingTs = pts100ns;
    frame.frameData = pipeline->audioBuffer;
    frame.trackId = session->audioTrackId;

    int result = 0;
    {
        auto begin_ = std::chrono::steady_clock::now();
        defer({
                  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - begin_).count();
                  if (duration > 10) {
                      LOGW("[OA] EasyRTC_SendFrame took %lld ms", static_cast<long long>(duration));
                  }
              });
        result = EasyRTC_SendFrame(pipeline->audioTransceiver, &frame);
    }

    if (result != 0) {
        LOGE("EasyRTC_SendFrame (audio) failed: %d", result);
    }else {
        session->audioFramesSent.fetch_add(1, std::memory_order_relaxed);
        FLOGI("[OA] EasyRTC_SendFrame (audio) success: size=%u, pts=%llu", frame.size, static_cast<unsigned long long>(frame.presentationTs));
    }
#endif


    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void errorCallback(
        AAudioStream* stream,
        void* userData,
        aaudio_result_t error) {
    auto session = static_cast<MediaSession*>(userData);
    if (!session || !session->audioCapture) {
        LOGE("AAudio capture error: %d", error);
        return;
    }
    auto pipeline = session->audioCapture;
    if (error != AAUDIO_ERROR_DISCONNECTED) {
        LOGE("AAudio capture error: %d", error);
        return;
    }

    LOGW("[AUDIO] AAudio capture stream disconnected, scheduling reconnect");

    std::thread([session, pipeline]() {
        {
            std::lock_guard<std::mutex> lock(pipeline->mutex);
            if (pipeline->stream) {
                AAudioStream_requestStop(pipeline->stream);
                AAudioStream_close(pipeline->stream);
                pipeline->stream = nullptr;
            }
        }

        for (int attempt = 1; attempt <= 5; ++attempt) {
            if (!pipeline->running.load()) {
                break;
            }
            if (openAndStartCaptureStream(session, pipeline) == 0) {
                LOGI("[AUDIO] Capture reconnect success on attempt %d", attempt);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
        }

        LOGE("[AUDIO] Capture reconnect failed");
    }).detach();
}

int audioCaptureStart(MediaSession* session) {
    assert(session);
    auto pipeline = session->audioCapture;
    if (!pipeline) return -1;

    LOGI("[CRITICAL] AudioCaptureStart: %dHz %dch", SAMPLE_RATE, CHANNEL_COUNT);

    pipeline->running.store(true);

    if (openAndStartCaptureStream(session, pipeline) != 0) {
        pipeline->running.store(false);
        return -1;
    }
    return 0;
}

void audioCaptureStop(MediaSession *session) {
    assert(session);
    auto pipeline = session->audioCapture;
    if (!pipeline) return;

    LOGI("[OA] AudioCaptureStop: stream=%p", pipeline->stream);
    pipeline->running.store(false);

    AAudioStream* stream = nullptr;
    {
        LOGI("[OA] AudioCaptureStop: reset pipeline stream");
        std::lock_guard<std::mutex> lock(pipeline->mutex);
        stream = pipeline->stream;
        pipeline->stream = nullptr;
    }

    if (stream) {
        LOGI("[OA] AudioCaptureStop: AAudioStream_requestStop");
        AAudioStream_requestStop(stream);
        aaudio_result_t result = AAUDIO_OK;
        aaudio_stream_state_t currentState = AAudioStream_getState(stream);
        aaudio_stream_state_t inputState = currentState;
        LOGI("[OA] AudioCaptureStop: currentState=%d, we need stopped(%d)", currentState, AAUDIO_STREAM_STATE_STOPPED);
        while (result == AAUDIO_OK && currentState != AAUDIO_STREAM_STATE_STOPPED) {
            LOGI("[OA] AudioCaptureStop: waiting for stream to stop...");
            result = AAudioStream_waitForStateChange(stream, inputState, &currentState, 100000);
            inputState = currentState;
            LOGI("[OA] AudioCaptureStop: waiting for stream to stop: currentState=%d, we need stopped(%d)", currentState, AAUDIO_STREAM_STATE_STOPPED);
        }
        LOGI("[OA] AudioCaptureStop: AAudioStream_close");
        AAudioStream_close(stream);
    }
    session->audioCapture = nullptr;

    LOGI("[OA] AudioCaptureStop: DONE");
}