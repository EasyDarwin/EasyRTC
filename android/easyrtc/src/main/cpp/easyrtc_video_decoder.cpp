#include "easyrtc_video_decoder.h"
#include <cstring>
#include <unistd.h>
#include <atomic>

namespace {
std::atomic<uint64_t> gDecQueued{0};
std::atomic<uint64_t> gDecRendered{0};
std::atomic<uint64_t> gDecTryLater{0};
std::atomic<uint64_t> gDecFormatChanged{0};
}

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
    int enqueued{};
    while (pipeline->running.load() && !pipeline->destroyed.load()) {
        VideoDecoderPipeline::Packet packet;
        {
            std::lock_guard<std::mutex> lock(pipeline->queueMutex);
            if (!pipeline->frameQueue.empty()) {
                packet = std::move(pipeline->frameQueue.front());
                pipeline->frameQueue.pop();
            }
        }

        if (packet.data.empty()) {
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
        if (!inputBuf || packet.data.size() > outSize) {
            LOGW("Invalid decoder input buffer: inputBuf=%p frame=%zu cap=%zu",
                 inputBuf, packet.data.size(), outSize);
            continue;
        }

        memcpy(inputBuf, packet.data.data(), packet.data.size());
        int flags = 0;
        if (packet.frameFlags & EASYRTC_FRAME_FLAG_KEY_FRAME) {
            flags |= AMEDIACODEC_BUFFER_FLAG_KEY_FRAME;
        }
        AMediaCodec_queueInputBuffer(decoder, inputBufId, 0, packet.data.size(), packet.ptsUs, flags);
        uint64_t q = gDecQueued.fetch_add(1) + 1;
        pipeline->enqueuedFrames.store(q);
        enqueued++;
        if (q <= 8 || (q % 120) == 0) {
            LOGD("DEC_IN q=%llu size=%zu pts=%lld flags=0x%x queue_remain=%zu", 
                 static_cast<unsigned long long>(q),
                 packet.data.size(),
                 static_cast<long long>(packet.ptsUs),
                 packet.frameFlags,
                 pipeline->frameQueue.size());
        }

        while (pipeline->running.load() && !pipeline->destroyed.load()) {
            ssize_t outputBufId = AMediaCodec_dequeueOutputBuffer(decoder, &bufferInfo,
                    pipeline->DEQUEUE_TIMEOUT_US);
            if (outputBufId >= 0) {
                AMediaCodec_releaseOutputBuffer(decoder, outputBufId, true);
                pipeline->errorCount.store(0);
                uint64_t r = gDecRendered.fetch_add(1) + 1;
                pipeline->renderedFrames.store(r);
                enqueued--;
                if (r <= 8 || (r % 120) == 0) {
                    LOGD("DEC_OUT r=%llu size=%d pts=%lld flags=0x%x, codec cached:%d",
                         static_cast<unsigned long long>(r),
                         bufferInfo.size,
                         static_cast<long long>(bufferInfo.presentationTimeUs),
                         bufferInfo.flags, enqueued);
                }
                break;
            } else if (outputBufId == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                uint64_t t = gDecTryLater.fetch_add(1) + 1;
                pipeline->tryLaterCount.store(t);
                if (t <= 8 || (t % 240) == 0) {
                    LOGD("DEC_TRY_LATER count=%llu", static_cast<unsigned long long>(t));
                }
                break;
            } else if (outputBufId == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                uint64_t f = gDecFormatChanged.fetch_add(1) + 1;
                AMediaFormat* outFormat = AMediaCodec_getOutputFormat(decoder);
                if (outFormat) {
                    int32_t w = 0, h = 0;
                    AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_WIDTH, &w);
                    AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_HEIGHT, &h);
                    LOGD("DEC_FMT_CHANGED count=%llu %dx%d", static_cast<unsigned long long>(f), w, h);
                    if (w > 0 && h > 0 && (w != pipeline->width || h != pipeline->height)) {
                        pipeline->width = w;
                        pipeline->height = h;
                        if (pipeline->onVideoSize) {
                            pipeline->onVideoSize(pipeline->onVideoSizeUserPtr, w, h);
                        }
                    }
                    AMediaFormat_delete(outFormat);
                } else {
                    LOGD("DEC_FMT_CHANGED count=%llu", static_cast<unsigned long long>(f));
                }
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
    pthread_t th = pipeline->decodeThread.native_handle();
    sched_param sch_params;
    sch_params.sched_priority = 0;//sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(th, SCHED_FIFO, &sch_params);
    return 0;
}

void videoDecoderEnqueueFrame(VideoDecoderPipeline* pipeline, const uint8_t* data, int32_t size, int64_t ptsUs, uint32_t frameFlags) {
    if (!pipeline || !pipeline->running.load() || pipeline->destroyed.load() || size <= 0) return;

    VideoDecoderPipeline::Packet packet;
    packet.data.assign(data, data + size);
    packet.ptsUs = ptsUs;
    packet.frameFlags = frameFlags;
    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        if (pipeline->frameQueue.size() >= pipeline->MAX_QUEUE_SIZE) {
            pipeline->frameQueue.pop();
        }
        pipeline->frameQueue.push(std::move(packet));
    }
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
