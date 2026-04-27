#include "easyrtc_audio.h"
#include <aaudio/AAudio.h>
#include <cstring>
#include <time.h>
#include <assert.h>
#include "easyrtc_session.h"

static constexpr int32_t SAMPLE_RATE = 8000;
static constexpr int32_t CHANNEL_COUNT = 1;

static aaudio_data_callback_result_t dataCallback(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames) {
    auto session = static_cast<MediaSession*>(userData);
    auto* pipeline = session->audioCapture.get();
    if (!pipeline || !pipeline->running.load()) {
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
    UINT64 pts = 0;
    if (tsResult == AAUDIO_OK) {
        pts = static_cast<UINT64>(timeNanos / 100);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        pts = static_cast<UINT64>((ts.tv_sec * 1000000000LL + ts.tv_nsec) / 100);
    }

    EasyRTC_Frame frame{};
    frame.version = 0;
    frame.size = static_cast<UINT32>(dataSize);
    frame.flags = static_cast<EasyRTC_FRAME_FLAGS>(EASYRTC_FRAME_FLAG_NONE);
    frame.presentationTs = pts;
    frame.decodingTs = pts;
    frame.frameData = pcmData;
    frame.trackId = 0;

    // LOGD("Audio capture callback: frames=%d, size=%d, pts=%llu", numFrames, dataSize, static_cast<unsigned long long>(pts));
    int result = EasyRTC_SendFrame(pipeline->audioTransceiver, &frame);
    if (result != 0) {
        LOGE("EasyRTC_SendFrame (audio) failed: %d", result);
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void errorCallback(
        AAudioStream* stream,
        void* userData,
        aaudio_result_t error) {
    LOGE("AAudio capture error: %d", error);
}

int audioCaptureStart(MediaSession* session) {
    assert(session);
    auto pipeline = session->audioCapture;
    if (!pipeline) return -1;

    LOGI("[CRITICAL] AudioCaptureStart: %dHz %dch", SAMPLE_RATE, CHANNEL_COUNT);

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
    AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
    AAudioStreamBuilder_setDataCallback(builder, dataCallback, session);
    AAudioStreamBuilder_setErrorCallback(builder, errorCallback, session);

    AAudioStream* stream = nullptr;
    result = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || !stream) {
        LOGE("Failed to open AAudio input stream: %d", result);
        return -1;
    }

    pipeline->stream = stream;
    pipeline->running.store(true);

    result = AAudioStream_requestStart(stream);
    if (result != AAUDIO_OK) {
        LOGE("Failed to start AAudio input stream: %d", result);
        AAudioStream_close(stream);
        pipeline->stream = nullptr;
        pipeline->running.store(false);
        return -1;
    }

    LOGI("[CRITICAL] AudioCaptureStart: SUCCESS stream=%p", stream);
    return 0;
}

void audioCaptureStop(MediaSession *session) {
    assert(session);
    auto pipeline = session->audioCapture;
    if (!pipeline) return;

    LOGI("[CRITICAL] AudioCaptureStop: stream=%p", pipeline->stream);
    pipeline->running.store(false);

    if (pipeline->stream) {
        // {
        //     AAudioStream_requestFlush(pipeline->stream);
        //     // AAudioStream_waitForStateChange(pipeline->stream, AAUDIO_STREAM_STATE_STARTED, AAUDIO_STREAM_STATE_STOPPED, nullptr, nullptr);
        //     aaudio_result_t result = AAUDIO_OK;
        //     aaudio_stream_state_t currentState = AAudioStream_getState(pipeline->stream);
        //     aaudio_stream_state_t inputState = currentState;
        //     while (result == AAUDIO_OK && currentState != AAUDIO_STREAM_STATE_FLUSHED) {
        //         result = AAudioStream_waitForStateChange(
        //                                     pipeline->stream, inputState, &currentState, 100000);
        //         inputState = currentState;
        //     }
        // }
        {
            AAudioStream_requestStop(pipeline->stream);
            // AAudioStream_waitForStateChange(pipeline->stream, AAUDIO_STREAM_STATE_STARTED, AAUDIO_STREAM_STATE_STOPPED, nullptr, nullptr);
            aaudio_result_t result = AAUDIO_OK;
            aaudio_stream_state_t currentState = AAudioStream_getState(pipeline->stream);
            aaudio_stream_state_t inputState = currentState;
            while (result == AAUDIO_OK && currentState != AAUDIO_STREAM_STATE_STOPPED) {
                result = AAudioStream_waitForStateChange(
                                            pipeline->stream, inputState, &currentState, 100000);
                inputState = currentState;
            }
        }
        {
            AAudioStream_close(pipeline->stream);
            aaudio_result_t result = AAUDIO_OK;
            aaudio_stream_state_t currentState = AAudioStream_getState(pipeline->stream);
            aaudio_stream_state_t inputState = currentState;
            while (result == AAUDIO_OK && currentState != AAUDIO_STREAM_STATE_CLOSED) {
                result = AAudioStream_waitForStateChange(
                                            pipeline->stream, inputState, &currentState, 100000);
                inputState = currentState;
            }
        }
        pipeline->stream = nullptr;
    }

    LOGI("[CRITICAL] AudioCaptureStop: DONE");
}