#include "codec/video_decoder.h"
#include "session/common.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include "utils/defer.hpp"
#include <media/NdkMediaFormat.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <functional>
#include "session/media_session.h"
#include "util/video.hpp"

namespace {
std::atomic<uint64_t> gDecQueued{0};
std::atomic<uint64_t> gDecRendered{0};
std::atomic<uint64_t> gDecTryLater{0};
std::atomic<uint64_t> gDecFormatChanged{0};
}

static bool initDecoder(const std::shared_ptr<VideoDecoderPipeline> &pipeline) {
    assert(!pipeline->decoder);
    auto decoder = AMediaCodec_createDecoderByType(pipeline->currentCodecType.c_str());
    if (!decoder) {
        LOGE("Failed to create video decoder for %s", pipeline->currentCodecType.c_str());
        assert(false);
        return false;
    }
    auto decoder_ptr = std::shared_ptr<AMediaCodec>(decoder, [](AMediaCodec* ptr) {
        if (ptr) {
            AMediaCodec_delete(ptr);
        }
    });
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, pipeline->currentCodecType.c_str());
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, pipeline->width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, pipeline->height);
//    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, pipeline->frameRate);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 19);
    if (!pipeline->csd0.empty()) {
        AMediaFormat_setBuffer(format, "csd-0", pipeline->csd0.data(), pipeline->csd0.size());
    }
    if (!pipeline->csd1.empty()) {
        AMediaFormat_setBuffer(format, "csd-1", pipeline->csd1.data(), pipeline->csd1.size());
    }

    LOGI("Configuring video decoder: %s %dx%d, frame rate: %d", pipeline->currentCodecType.c_str(),
         pipeline->width, pipeline->height, pipeline->frameRate);
    media_status_t status = AMediaCodec_configure(decoder, format,
            pipeline->surface, nullptr, 0);
    AMediaFormat_delete(format);

    if (status != AMEDIA_OK) {
        LOGE("Failed to configure video decoder: %d", status);
        assert(false);
        return false;
    }

    status = AMediaCodec_start(decoder);
    if (status != AMEDIA_OK) {
        LOGE("Failed to start video decoder: %d", status);
        assert(false);
        return false;
    }

    pipeline->decoder = decoder_ptr;
    pipeline->errorCount.store(0);
    LOGI("Video decoder initialized: %s %dx%d, frame rate: %d", pipeline->currentCodecType.c_str(),
            pipeline->width, pipeline->height, pipeline->frameRate);
    return true;
}

int64_t getMonoUs() {
    using namespace std::chrono;
    static uint64_t _start_us = 0;
    if (_start_us == 0) {
        _start_us = duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()
        ).count();
    }
    return static_cast<int64_t>(duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()
    ).count() - _start_us);
}

static void sleepInterruptibleUs(int64_t targetSleepUs,
                                 const std::function<bool()> &shouldBreak) {
    if (targetSleepUs <= 0) {
        return;
    }
    static constexpr int64_t kMaxSleepUsPerSlice = 20000;
    const auto sleepStart = std::chrono::steady_clock::now();

    while (true) {
        if (shouldBreak && shouldBreak()) {
            break;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
                now - sleepStart).count();
        if (elapsedUs >= targetSleepUs) {
            break;
        }
        const int64_t remainingUs = targetSleepUs - elapsedUs;
        const int64_t sliceUs = std::min(remainingUs, kMaxSleepUsPerSlice);
        std::this_thread::sleep_for(std::chrono::microseconds(sliceUs));
    }
}


std::shared_ptr<VideoDecoderPipeline> videoDecoderCreate(ANativeWindow* surface, int codecType, int width, int height) {
    auto pipeline = std::make_shared<VideoDecoderPipeline>();
    pipeline->width = 0;
    pipeline->height = 0;
    pipeline->currentCodecType = (codecType == EasyRTC_CODEC_H265) ? "video/hevc" : "video/avc";

    if (surface) {
        pipeline->surface = surface;
    }

    LOGI("VideoDecoderPipeline created: %s %dx%d", pipeline->currentCodecType.c_str(), width, height);
    return pipeline;
}


int64_t fixSleepTime(int64_t sleepTimeUs, int64_t totalTimestampDifferUs, int64_t delayUs) {
    if (totalTimestampDifferUs < 0) {
        // LOGW("totalTimestampDifferUs is:%lld, this should not be happen.", static_cast<long long>(totalTimestampDifferUs));
        totalTimestampDifferUs = 0;
    }

    double dValue = ((double) (delayUs - totalTimestampDifferUs)) / 1000000.0;
    double radio = std::exp(dValue);
    double r = sleepTimeUs * radio + 0.5;
    // LOGI("%lld,%lld,%lld->%lld", static_cast<long long>(sleepTimeUs), static_cast<long long>(totalTimestampDifferUs), static_cast<long long>(delayUs), static_cast<long long>(r));
    return static_cast<int64_t>(r);
}

int videoDecoderStart(std::shared_ptr<VideoDecoderPipeline> pipeline) {
    pipeline->running.store(true);
    pipeline->decodeThread = std::thread([pipeline]() {
        LOGI("Video decode thread started");
        AMediaCodecBufferInfo bufferInfo;
        int enqueued{};
        int64_t firstPtsUs = -1;
        int64_t firstSystemUs = -1;
        bool droppingPackets = false;


        auto enqueueDecoder = [&](AMediaCodec *decoder, Packet *packet) {
            ssize_t inputBufId = AMediaCodec_dequeueInputBuffer(decoder, pipeline->DEQUEUE_TIMEOUT_US);
            if (inputBufId < 0) {
                return false;
            }
            size_t outSize = 0;
            uint8_t* inputBuf = AMediaCodec_getInputBuffer(decoder, inputBufId, &outSize);
            if (!inputBuf || packet->size > outSize) {
                LOGW("Invalid decoder input buffer: inputBuf=%p frame=%u cap=%zu",
                     inputBuf, packet->size, outSize);
                assert(false);
                return true;
            }

            memcpy(inputBuf, packet->data(), packet->size);
            int flags = 0;
            if (packet->frameFlags & EASYRTC_FRAME_FLAG_KEY_FRAME) {
                flags |= AMEDIACODEC_BUFFER_FLAG_KEY_FRAME;
            }
            static uint32_t  frame_idx = 0;
            if (frame_idx == 0) frame_idx = packet->index;
            else {
                if (frame_idx != packet->index -1){
                    LOGW("packet not continouse.%u->%u", frame_idx, packet->index);
                }
                frame_idx = packet->index;
            }
            AMediaCodec_queueInputBuffer(decoder, inputBufId, 0, packet->size, packet->ptsUs, flags);
            uint64_t q = gDecQueued.fetch_add(1) + 1;
            pipeline->enqueuedFrames.store(q);
            enqueued++;
            if (q <= 8 || (q % 120) == 0) {
                LOGI("[VI] DEC_IN q=%llu size=%u pts=%lld flags=0x%x PKT queue cached=%zu",
                     static_cast<unsigned long long>(q),
                     packet->size,
                     static_cast<long long>(packet->ptsUs),
                     packet->frameFlags,
                     pipeline->frameQueue.size());
            }
            return true;
        };

        auto dequeueDecoder = [&](AMediaCodec *decoder) -> bool {
            ssize_t outputBufId = AMediaCodec_dequeueOutputBuffer(decoder, &bufferInfo,
                                                                  pipeline->DEQUEUE_TIMEOUT_US);
            if (outputBufId >= 0) {
                if (firstPtsUs < 0) {
                    firstPtsUs = bufferInfo.presentationTimeUs;
                    firstSystemUs = getMonoUs();
                }

                const int64_t ptsDurationUs = bufferInfo.presentationTimeUs - firstPtsUs;
                int64_t masterClockDurationUs = getMonoUs() - firstSystemUs;
                if (pipeline->audio_master_clock_us_from_begining_to_now > 0) {
                    masterClockDurationUs = pipeline->audio_master_clock_us_from_begining_to_now;
                }
                const auto delta_us_to_master = (ptsDurationUs - masterClockDurationUs);
                FLOGI("[VI] DEC_OUT process:%lldms, master clock:%lldms, delta:%lldms", ptsDurationUs/1000, masterClockDurationUs/1000, delta_us_to_master/1000);
                int64_t sleepUs = delta_us_to_master - 3000;
                auto fixedSleepUs = fixSleepTime(sleepUs, pipeline->frameQueue.cached_us(), 0);
                FLOGI("[VI] DEC_OUT before fix sleepUs=%lld, after fix sleepUs=%lld, cached_packet_us=%lld", sleepUs, fixedSleepUs, pipeline->frameQueue.cached_us());
                if (sleepUs > 0) {
                    sleepInterruptibleUs(sleepUs, [&]() {
                        int64_t masterClockDurationUs = getMonoUs() - firstSystemUs;
                        if (pipeline->audio_master_clock_us_from_begining_to_now > 0) {
                            masterClockDurationUs = pipeline->audio_master_clock_us_from_begining_to_now;
                        }
                        const auto delta_us_to_master = (ptsDurationUs - masterClockDurationUs);
                        return !pipeline->running.load() || delta_us_to_master < 10000;
                    });
                }
                AMediaCodec_releaseOutputBuffer(decoder, outputBufId, true);
                pipeline->errorCount.store(0);
                uint64_t r = gDecRendered.fetch_add(1) + 1;
                pipeline->renderedFrames.store(r);
                enqueued--;
                if (r <= 8 || (r % 120) == 0) {
                    LOGI("DEC_OUT r=%llu size=%d pts=%lld sleepUs=%lld flags=0x%x codec queue cached:%d",
                         static_cast<unsigned long long>(r),
                         bufferInfo.size,
                         static_cast<long long>(bufferInfo.presentationTimeUs),
                         static_cast<long long>(sleepUs),
                         bufferInfo.flags, enqueued);
                }
                return true;
            } else if (outputBufId == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                uint64_t t = gDecTryLater.fetch_add(1) + 1;
                pipeline->tryLaterCount.store(t);
                if (t <= 8 || (t % 240) == 0) {
                    LOGI("DEC_TRY_LATER count=%llu", static_cast<unsigned long long>(t));
                }
            } else if (outputBufId == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                uint64_t f = gDecFormatChanged.fetch_add(1) + 1;
                AMediaFormat* outFormat = AMediaCodec_getOutputFormat(decoder);
                if (outFormat) {
                    int32_t w = 0, h = 0;
                    AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_WIDTH, &w);
                    AMediaFormat_getInt32(outFormat, AMEDIAFORMAT_KEY_HEIGHT, &h);
                    LOGI("DEC_FMT_CHANGED count=%llu %dx%d", static_cast<unsigned long long>(f), w, h);
//                    if (w > 0 && h > 0 && (w != pipeline->width || h != pipeline->height))
                    {
                        pipeline->width = w;
                        pipeline->height = h;
                        if (pipeline->onVideoSize) {
                            pipeline->onVideoSize(pipeline->onVideoSizeUserPtr, w, h);
                        }
                    }
                    AMediaFormat_delete(outFormat);
                } else {
                    LOGI("DEC_FMT_CHANGED count=%llu", static_cast<unsigned long long>(f));
                }
                return true;
            }
            return false;
        };

        auto setupDecoder = [&]() {
            auto packet = pipeline->frameQueue.acquirePop();
            if (!packet) {
                sleepInterruptibleUs(1000, [&]() {
                    return !pipeline->running.load();
                });
                return false;
            }
            bool decoderOK = false;
            defer(if (!decoderOK) pipeline->frameQueue.commitPop());
            if (packet->frameFlags & EASYRTC_FRAME_FLAG_KEY_FRAME) {
                auto hex = uint8_to_hex(packet->data(), std::min(uint32_t(56), packet->size));
                LOGI("Frame:%s", hex.c_str());

                bool isHEVC = (pipeline->currentCodecType == "video/hevc");

                if (isHEVC) {
                    extractH265VpsSpsPpsFromAnnexb(packet->data(), packet->size, pipeline->csd0);
                    pipeline->csd1.clear();
                    auto csd0Hex = uint8_to_hex(pipeline->csd0.data(), pipeline->csd0.size());
                    LOGI("HEVC csd0(VPS+SPS+PPS)=%s", csd0Hex.c_str());
                } else {
                    extractSpsPpsFromAnnexb(packet->data(), std::min(packet->size, uint32_t(256)), pipeline->csd0, pipeline->csd1);
                    auto csd0Hex = uint8_to_hex(pipeline->csd0.data(), pipeline->csd0.size());
                    auto csd1Hex = uint8_to_hex(pipeline->csd1.data(), pipeline->csd1.size());
                    LOGI("AVC csd0=%s, csd1=%s", csd0Hex.c_str(), csd1Hex.c_str());
                }

                if (pipeline->width <= 0 || pipeline->height <= 0) {
                    pipeline->width = 1920;
                    pipeline->height = 1080;
                }
                LOGI("Decoder configure size:%dx%d (actual from FORMAT_CHANGED)", pipeline->width, pipeline->height);

                if (initDecoder(pipeline)){
                    decoderOK = true;
//                    auto sureConsumed = enqueueDecoder(pipeline->decoder.get(), packet);
//                    assert(sureConsumed && "Failed to enqueue key frame after decoder init");
                    return true;
                }else {
                    assert(false && "initDecoder failed!");
                }
            } else {
                FLOGI("Dropping non-key frame since decoder is not initialized, pts=%lld", static_cast<long long>(packet->ptsUs));
            }
            return false;
        };

        while (pipeline->running.load()) {
            if (!pipeline->decoder) {
                if (!setupDecoder()) {
                    continue;
                }
                assert(pipeline->decoder);
            }
            auto decoder = pipeline->decoder.get();
            assert(pipeline->decoder);
            while (pipeline->running.load()) {
                if (dequeueDecoder(decoder)) {
                    continue;
                }else {
                    break;
                }
            }

            auto packet = pipeline->frameQueue.acquirePop();
            if (!packet) {
                sleepInterruptibleUs(1000, [&]() {
                    return !pipeline->running.load();
                });
                continue;
            }
            bool sureConsumed = true;
            defer(if (sureConsumed) pipeline->frameQueue.commitPop());
            FLOGI("[VI] VIDEO PKT OUT pts:%llums, in PKT caches:%llu", packet->ptsUs/1000, pipeline->frameQueue.size());
            if (droppingPackets && (packet->frameFlags & EASYRTC_FRAME_FLAG_KEY_FRAME) != EASYRTC_FRAME_FLAG_KEY_FRAME) {
                LOGW("Dropping packet pts=%lld", static_cast<long long>(packet->ptsUs));
                continue;
            }
            droppingPackets = false;
#if 0
            const auto cached_packet_millis =  pipeline->frameQueue.cached_us()/1000;
        if (cached_packet_millis > 500 && pipeline->frameQueue.size() > 30) {
            bool hasKeyInside = false;
            pipeline->frameQueue.check([&](const Packet* p) {
                if(!p) return false;
                if (p->frameFlags & EASYRTC_FRAME_FLAG_KEY_FRAME) {
                    hasKeyInside = true;
                    return true;
                }
                return false;
            });
            if (hasKeyInside) {
                LOGW("Too many pending frames: %zu(count), %llu(ms), start dropping non-key frames", pipeline->frameQueue.size(), cached_packet_millis);
                droppingPackets = true;
                continue;
            }
        }
        assert(!droppingPackets);
#endif
            sureConsumed = enqueueDecoder(decoder, packet);
        }

        if (pipeline->decoder) {
            AMediaCodec_stop(pipeline->decoder.get());
            pipeline->decoder = nullptr;
        }
        LOGI("Video decode thread exiting");
    });
    pthread_t th = pipeline->decodeThread.native_handle();
    sched_param sch_params;
    sch_params.sched_priority = 0;//sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(th, SCHED_FIFO, &sch_params);
    return 0;
}

void videoDecoderEnqueueFrame(std::shared_ptr<VideoDecoderPipeline> pipeline, const EasyRTC_Frame* frame) {
    // const uint8_t* data, int32_t size, int64_t ptsUs, uint32_t frameFlags
    const uint8_t* data = frame->frameData;
    int32_t size = frame->size;
    int64_t ptsUs = VIDEO_PTS_TO_US(frame->presentationTs);
    uint32_t frameFlags = frame->flags;
    if (!pipeline || !pipeline->running.load() || size <= 0) return;
    Packet *slot = pipeline->frameQueue.acquirePush();
    if (!slot) {
        LOGW("no available VIDEO cache, waiting...");
        do {
            if (!pipeline->running.load()) {
                return;
            }
            sleepInterruptibleUs(10000, [&]() {
                return !pipeline->running.load();
            });
            slot = pipeline->frameQueue.acquirePush();
        } while (!slot);
    }
    slot->setData(data, static_cast<uint32_t>(size));
    slot->ptsUs = ptsUs;
    slot->frameFlags = frameFlags;
    slot->index = frame->index;
    pipeline->frameQueue.commitPush();
    {
        static int64_t __idx = 0;
        if (__idx++ % 300 == 0) {
            int64_t firstUs{}, lastUs{};
            pipeline->frameQueue.check([&](const Packet*pkt){
                if (!pkt) return false;
                if (firstUs == 0) firstUs = pkt->ptsUs;
                lastUs = pkt->ptsUs;
                return false;
            });
            LOGI("VIDEO PKG IN pts:%llums, in packet caches:%llu/%llums", ptsUs/1000, pipeline->frameQueue.size(), (lastUs - firstUs)/1000);
        }
    }
}

void videoDecoderRelease(std::shared_ptr<VideoDecoderPipeline> pipeline) {
    if (!pipeline) return;
    pipeline->running.store(false);
    if (pipeline->decodeThread.joinable()) {
        pipeline->decodeThread.join();
    }
}
