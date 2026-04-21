#include "easyrtc_video_decoder.h"
#include <cstring>
#include <unistd.h>

static bool initDecoder(VideoDecoderPipeline* pipeline) {
    std::lock_guard<std::mutex> lock(pipeline->decoderMutex);

    if (pipeline->decoder) {
        AMediaCodec_stop(pipeline->decoder);
        AMediaCodec_delete(pipeline->decoder);
        pipeline->decoder = nullptr;
    }

    pipeline->decoder = AMediaCodec_createDecoderByType(pipeline->currentCodecType.c_str());
    if (!pipeline->decoder) {
        LOGE("Failed to create video decoder for %s", pipeline->currentCodecType.c_str());
        return false;
    }

    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, pipeline->currentCodecType.c_str());
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, pipeline->width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, pipeline->height);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, pipeline->frameRate);

    media_status_t status = AMediaCodec_configure(pipeline->decoder, format,
            pipeline->surface, nullptr, 0);
    AMediaFormat_delete(format);

    if (status != AMEDIA_OK) {
        LOGE("Failed to configure video decoder: %d", status);
        AMediaCodec_delete(pipeline->decoder);
        pipeline->decoder = nullptr;
        return false;
    }

    status = AMediaCodec_start(pipeline->decoder);
    if (status != AMEDIA_OK) {
        LOGE("Failed to start video decoder: %d", status);
        AMediaCodec_delete(pipeline->decoder);
        pipeline->decoder = nullptr;
        return false;
    }

    pipeline->errorCount.store(0);
    LOGD("Video decoder initialized: %s %dx%d", pipeline->currentCodecType.c_str(),
            pipeline->width, pipeline->height);
    return true;
}

static void* decodeThreadFunc(void* arg) {
    auto* pipeline = static_cast<VideoDecoderPipeline*>(arg);
    LOGD("Video decode thread started");
    AMediaCodecBufferInfo bufferInfo;

    while (pipeline->running.load() && !pipeline->destroyed.load()) {
        std::vector<uint8_t> frameData;
        {
            std::lock_guard<std::mutex> lock(pipeline->queueMutex);
            if (!pipeline->frameQueue.empty()) {
                frameData = std::move(pipeline->frameQueue.front());
                pipeline->frameQueue.pop();
            }
        }

        if (frameData.empty()) {
            usleep(1000);
            continue;
        }

        AMediaCodec* decoder = pipeline->decoder;
        if (!decoder) {
            usleep(5000);
            continue;
        }

        ssize_t inputBufId = AMediaCodec_dequeueInputBuffer(decoder, pipeline->DEQUEUE_TIMEOUT_US);
        if (inputBufId < 0) {
            continue;
        }

        size_t outSize = 0;
        uint8_t* inputBuf = AMediaCodec_getInputBuffer(decoder, inputBufId, &outSize);
        if (!inputBuf || frameData.size() > outSize) {
            LOGW("Invalid decoder input buffer: inputBuf=%p frame=%zu cap=%zu",
                 inputBuf, frameData.size(), outSize);
            continue;
        }

        memcpy(inputBuf, frameData.data(), frameData.size());
        AMediaCodec_queueInputBuffer(decoder, inputBufId, 0, frameData.size(), 0, 0);

        while (pipeline->running.load() && !pipeline->destroyed.load()) {
            ssize_t outputBufId = AMediaCodec_dequeueOutputBuffer(decoder, &bufferInfo,
                    pipeline->DEQUEUE_TIMEOUT_US);
            if (outputBufId >= 0) {
                AMediaCodec_releaseOutputBuffer(decoder, outputBufId, true);
                pipeline->errorCount.store(0);
                break;
            } else if (outputBufId == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                break;
            } else if (outputBufId == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                LOGD("Decoder output format changed");
            }
        }
    }

    LOGD("Video decode thread exiting");
    return nullptr;
}

VideoDecoderPipeline* videoDecoderCreate(ANativeWindow* surface, int codecType, int width, int height) {
    auto* pipeline = new VideoDecoderPipeline();
    pipeline->width = width;
    pipeline->height = height;
    pipeline->currentCodecType = (codecType == 6) ? "video/hevc" : "video/avc";

    if (surface) {
        pipeline->surface = surface;
    }

    if (!initDecoder(pipeline)) {
        delete pipeline;
        return nullptr;
    }

    LOGD("VideoDecoderPipeline created: %s %dx%d", pipeline->currentCodecType.c_str(), width, height);
    return pipeline;
}

int videoDecoderStart(VideoDecoderPipeline* pipeline) {
    if (!pipeline) return -1;

    pipeline->running.store(true);
    pipeline->decodeThread = std::thread([pipeline]() { decodeThreadFunc(pipeline); });
    return 0;
}

void videoDecoderEnqueueFrame(VideoDecoderPipeline* pipeline, const uint8_t* data, int32_t size) {
    if (!pipeline || !pipeline->running.load() || pipeline->destroyed.load() || size <= 0) return;

    std::vector<uint8_t> frame(data, data + size);
    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        if (pipeline->frameQueue.size() >= pipeline->MAX_QUEUE_SIZE) {
            pipeline->frameQueue.pop();
        }
        pipeline->frameQueue.push(std::move(frame));
    }
}

void videoDecoderReinit(VideoDecoderPipeline* pipeline, int codecType) {
    if (!pipeline) return;

    std::string newCodec = (codecType == 6) ? "video/hevc" : "video/avc";
    if (newCodec == pipeline->currentCodecType) return;

    LOGD("Reinitializing video decoder: %s -> %s", pipeline->currentCodecType.c_str(), newCodec.c_str());

    pipeline->running.store(false);
    if (pipeline->decodeThread.joinable()) {
        pipeline->decodeThread.join();
    }

    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        while (!pipeline->frameQueue.empty()) pipeline->frameQueue.pop();
    }

    pipeline->currentCodecType = newCodec;
    initDecoder(pipeline);

    pipeline->running.store(true);
    pipeline->decodeThread = std::thread([pipeline]() { decodeThreadFunc(pipeline); });
}

void videoDecoderRelease(VideoDecoderPipeline* pipeline) {
    if (!pipeline) return;

    pipeline->running.store(false);
    pipeline->destroyed.store(true);

    if (pipeline->decodeThread.joinable()) {
        pipeline->decodeThread.join();
    }

    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        while (!pipeline->frameQueue.empty()) pipeline->frameQueue.pop();
    }

    {
        std::lock_guard<std::mutex> lock(pipeline->decoderMutex);
        if (pipeline->decoder) {
            AMediaCodec_stop(pipeline->decoder);
            AMediaCodec_delete(pipeline->decoder);
            pipeline->decoder = nullptr;
        }
    }

    if (pipeline->surface) {
        ANativeWindow_release(pipeline->surface);
        pipeline->surface = nullptr;
    }

    delete pipeline;
    LOGD("VideoDecoderPipeline released");
}
