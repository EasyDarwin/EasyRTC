#include "easyrtc_audio.h"
#include <cstring>
#include <time.h>

static constexpr int32_t SAMPLE_RATE = 8000;
static constexpr int32_t CHANNEL_COUNT = 1;

static aaudio_data_callback_result_t dataCallback(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames) {
    auto* pipeline = static_cast<AudioCapturePipeline*>(userData);
    if (!pipeline || !pipeline->running.load()) {
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    if (!pipeline->audioTransceiver) {
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

AudioCapturePipeline* audioCaptureCreate(EasyRTC_Transceiver audioTransceiver) {
    auto* pipeline = new AudioCapturePipeline();
    pipeline->audioTransceiver = audioTransceiver;
    LOGD("AudioCapturePipeline created, transceiver=%p", pipeline->audioTransceiver);
    return pipeline;
}

int audioCaptureStart(AudioCapturePipeline* pipeline) {
    if (!pipeline) return -1;

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
    AAudioStreamBuilder_setDataCallback(builder, dataCallback, pipeline);
    AAudioStreamBuilder_setErrorCallback(builder, errorCallback, pipeline);

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

    LOGD("Audio capture started: %dHz, %dch", SAMPLE_RATE, CHANNEL_COUNT);
    return 0;
}

void audioCaptureStop(AudioCapturePipeline* pipeline) {
    if (!pipeline) return;

    pipeline->running.store(false);

    if (pipeline->stream) {
        AAudioStream_requestStop(pipeline->stream);
        AAudioStream_close(pipeline->stream);
        pipeline->stream = nullptr;
    }

    LOGD("Audio capture stopped");
}

void audioCaptureRelease(AudioCapturePipeline* pipeline) {
    if (!pipeline) return;
    audioCaptureStop(pipeline);
    delete pipeline;
    LOGD("AudioCapturePipeline released");
}
