#include "easyrtc_audio_playback.h"
#include <cstring>

static aaudio_data_callback_result_t playbackCallback(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames) {
    auto* pipeline = static_cast<AudioPlaybackPipeline*>(userData);
    if (!pipeline || pipeline->stopped.load()) {
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    int32_t requestedBytes = numFrames * pipeline->CHANNEL_COUNT * sizeof(int16_t);
    auto* output = static_cast<uint8_t*>(audioData);

    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        if (!pipeline->jitterBuffer.empty()) {
            frame = std::move(pipeline->jitterBuffer.front());
            pipeline->jitterBuffer.pop();
        }
    }

    if (!frame.empty()) {
        int32_t toCopy = std::min(static_cast<int32_t>(frame.size()), requestedBytes);
        memcpy(output, frame.data(), toCopy);
        if (toCopy < requestedBytes) {
            memset(output + toCopy, 0, requestedBytes - toCopy);
        }
    } else {
        memset(output, 0, requestedBytes);
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void playbackErrorCallback(
        AAudioStream* stream,
        void* userData,
        aaudio_result_t error) {
    LOGE("AAudio playback error: %d", error);
}

static int ensureStreamCreated(AudioPlaybackPipeline* pipeline) {
    if (pipeline->stream) return 0;

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || !builder) {
        LOGE("Failed to create AAudio stream builder: %d", result);
        return -1;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(builder, pipeline->SAMPLE_RATE);
    AAudioStreamBuilder_setChannelCount(builder, pipeline->CHANNEL_COUNT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_VOICE_COMMUNICATION);
    AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_SPEECH);
    AAudioStreamBuilder_setDataCallback(builder, playbackCallback, pipeline);
    AAudioStreamBuilder_setErrorCallback(builder, playbackErrorCallback, pipeline);

    AAudioStream* stream = nullptr;
    result = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || !stream) {
        LOGE("Failed to open AAudio output stream: %d", result);
        return -1;
    }

    pipeline->stream = stream;
    pipeline->stopped.store(false);

    result = AAudioStream_requestStart(stream);
    if (result != AAUDIO_OK) {
        LOGE("Failed to start AAudio output stream: %d", result);
        AAudioStream_close(stream);
        pipeline->stream = nullptr;
        return -1;
    }

    pipeline->playing.store(true);
    LOGD("Audio playback started: %dHz, %dch", pipeline->SAMPLE_RATE, pipeline->CHANNEL_COUNT);
    return 0;
}

AudioPlaybackPipeline* audioPlaybackCreate() {
    auto* pipeline = new AudioPlaybackPipeline();
    LOGD("AudioPlaybackPipeline created");
    return pipeline;
}

void audioPlaybackEnqueueFrame(AudioPlaybackPipeline* pipeline, const uint8_t* data, int32_t size) {
    if (!pipeline || pipeline->stopped.load() || size <= 0) return;

    if (!pipeline->playing.load()) {
        ensureStreamCreated(pipeline);
    }

    std::vector<uint8_t> frame(data, data + size);
    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        if (pipeline->jitterBuffer.size() >= pipeline->MAX_QUEUE_SIZE) {
            pipeline->jitterBuffer.pop();
            LOGW("Audio playback queue overflow, dropped oldest frame");
        }
        pipeline->jitterBuffer.push(std::move(frame));
    }
}

void audioPlaybackRelease(AudioPlaybackPipeline* pipeline) {
    if (!pipeline) return;

    pipeline->stopped.store(true);
    pipeline->playing.store(false);

    if (pipeline->stream) {
        AAudioStream_requestStop(pipeline->stream);
        AAudioStream_close(pipeline->stream);
        pipeline->stream = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        while (!pipeline->jitterBuffer.empty()) {
            pipeline->jitterBuffer.pop();
        }
    }

    delete pipeline;
    LOGD("AudioPlaybackPipeline released");
}
