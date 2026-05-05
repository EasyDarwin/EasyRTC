#include "easyrtc_video_decoder.h"
#include "easyrtc_common.h"
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
#include "easyrtc_session.h"

namespace {
std::atomic<uint64_t> gDecQueued{0};
std::atomic<uint64_t> gDecRendered{0};
std::atomic<uint64_t> gDecTryLater{0};
std::atomic<uint64_t> gDecFormatChanged{0};
}

static bool initDecoder(VideoDecoderPipeline* pipeline) {
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
    LOGD("Video decoder initialized: %s %dx%d, frame rate: %d", pipeline->currentCodecType.c_str(),
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

static std::string uint8_to_hex(const uint8_t* data, size_t size) {
    std::string str;
    for (size_t i = 0; i < size; i++) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", data[i]);
        str += buf;
    }
    return str;
}

static void extractSpsPpsFromAnnexb(const uint8_t* data, size_t size, std::vector<uint8_t>& sps_data,  std::vector<uint8_t>& pps_data) {
    // This is a simplified parser for annexb format, which may not cover all cases.
    // It assumes the annexb data contains one SPS and one PPS, and they are in the format of:
    // [start code][NALU header][SPS][start code][NALU header][PPS]
    // the start code could be 0x00000001 or 0x000001
    sps_data.clear();
    pps_data.clear();
    size_t pos = 0;
    while (pos + 4 < size) {
        uint64_t startCode = (data[pos] << 24) | (data[pos + 1] << 16) | (data[pos + 2] << 8) | data[pos + 3];
        if (startCode == 0x00000001 || startCode == 0x000001) {
            const auto start_ = pos;
            // found 4-byte start code
            size_t nal_start = pos + (startCode == 0x00000001 ? 4 : 3);
            if (nal_start >= size) break;
            uint8_t nal_type = data[nal_start] & 0x1F;
            if (nal_type == 7 || nal_type == 8) {
                // SPS or PPS
                auto &target_data = (nal_type == 7) ? sps_data : pps_data;
                size_t nal_end = nal_start + 1;
                while (nal_end + 4 < size) {
                    uint64_t nextStartCode = (data[nal_end] << 24) | (data[nal_end + 1] << 16) | (data[nal_end + 2] << 8) | data[nal_end + 3];
                    if (nextStartCode == 0x00000001 || nextStartCode == 0x000001) {
                        break;
                    }
                    nal_end++;
                }
                target_data.insert(target_data.end(), data + start_, data + nal_end);
                pos = nal_end;
                auto hex = uint8_to_hex(target_data.data(), target_data.size());
                LOGD("Extracted %s, size=%zu, data=%s", (nal_type == 7) ? "SPS" : "PPS", target_data.size(), hex.c_str());
            }else {
                break;
            }
        } else {
            pos++;
        }
    }
}

static void* decodeThreadFunc(void* arg) {
    auto* pipeline = static_cast<VideoDecoderPipeline*>(arg);
    LOGD("Video decode thread started");
    AMediaCodecBufferInfo bufferInfo;
    int enqueued{};
    int64_t firstPtsUs = -1;
    int64_t firstSystemUs = -1;
    bool droppingPackets = false;


    auto enqueueDecoder = [&](AMediaCodec *decoder, Packet *packet) {
        ssize_t inputBufId = AMediaCodec_dequeueInputBuffer(decoder, pipeline->DEQUEUE_TIMEOUT_US);
        if (inputBufId < 0) {
            return;
        }
        size_t outSize = 0;
        uint8_t* inputBuf = AMediaCodec_getInputBuffer(decoder, inputBufId, &outSize);
        if (!inputBuf || packet->size > outSize) {
            LOGW("Invalid decoder input buffer: inputBuf=%p frame=%u cap=%zu",
                 inputBuf, packet->size, outSize);
            assert(false);
            return;
        }

        memcpy(inputBuf, packet->data(), packet->size);
        int flags = 0;
        if (packet->frameFlags & EASYRTC_FRAME_FLAG_KEY_FRAME) {
            flags |= AMEDIACODEC_BUFFER_FLAG_KEY_FRAME;
        }
        AMediaCodec_queueInputBuffer(decoder, inputBufId, 0, packet->size, packet->ptsUs, flags);
        uint64_t q = gDecQueued.fetch_add(1) + 1;
        pipeline->enqueuedFrames.store(q);
        enqueued++;
        if (q <= 8 || (q % 120) == 0) {
            LOGD("DEC_IN q=%llu size=%u pts=%lld flags=0x%x PKT queue cached=%zu",
                 static_cast<unsigned long long>(q),
                 packet->size,
                 static_cast<long long>(packet->ptsUs),
                 packet->frameFlags,
                 pipeline->frameQueue.size());
        }
    };

    auto dequeueDecoder = [&](AMediaCodec *decoder) -> bool {
        ssize_t outputBufId = AMediaCodec_dequeueOutputBuffer(decoder, &bufferInfo,
                    pipeline->DEQUEUE_TIMEOUT_US);
            if (outputBufId >= 0) {
                if (firstPtsUs < 0) {
                    firstPtsUs = bufferInfo.presentationTimeUs;
                    firstSystemUs = getMonoUs();
                }

                int64_t ptsUs = bufferInfo.presentationTimeUs - firstPtsUs;
                const auto delta_us_to_master = (ptsUs - pipeline->audio_master_clock_us_from_begining_to_now);
                FLOGI("VIDEO PKT OUT process:%lldms, audio master clock process:%lldms, delta:%lldms", ptsUs/1000, pipeline->audio_master_clock_us_from_begining_to_now/1000, delta_us_to_master/1000);
                int64_t sleepUs = delta_us_to_master - 3000;

                if (sleepUs > 0 ) {
                   std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
                }

                AMediaCodec_releaseOutputBuffer(decoder, outputBufId, true);
                pipeline->errorCount.store(0);
                uint64_t r = gDecRendered.fetch_add(1) + 1;
                pipeline->renderedFrames.store(r);
                enqueued--;
                if (r <= 8 || (r % 120) == 0) {
                    LOGD("DEC_OUT r=%llu size=%d pts=%lld sleepUs=%lld flags=0x%x codec queue cached:%d",
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
                    LOGD("DEC_TRY_LATER count=%llu", static_cast<unsigned long long>(t));
                }
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
            return false;
    };

    auto setupDecoder = [&]() {
        auto packet = pipeline->frameQueue.acquirePop();
        if (!packet) {
            usleep(1000);
            return false;
        }
        defer(pipeline->frameQueue.commitPop());
        if (packet->frameFlags != 0) {
            // extract sps/pps for avcc stream, and re-init decoder.
            // give me a util function to parse avcc stream and extract sps/pps data
            auto hex = uint8_to_hex(packet->data(), std::min(uint32_t(56), packet->size));
            LOGI("Frame:%s", hex.c_str());
             extractSpsPpsFromAnnexb(packet->data(), std::min(packet->size, uint32_t(256)), pipeline->csd0, pipeline->csd1);
            if (initDecoder(pipeline)){
                enqueueDecoder(pipeline->decoder.get(), packet);
                return true;
            }
        } else {
            FLOGI("Dropping non-key frame since decoder is not initialized, pts=%lld", static_cast<long long>(packet->ptsUs));
        }
        return false;
    };

    while (pipeline->running.load() && !pipeline->destroyed.load()) {
        if (!pipeline->decoder) {
            if (!setupDecoder()) {
                continue;
            }
        }
        auto decoder = pipeline->decoder.get();
        assert(pipeline->decoder);
        while (pipeline->running.load() && !pipeline->destroyed.load()) {
            if (dequeueDecoder(decoder)) {
                continue;
            }else {
                break;
            }
        }

        auto packet = pipeline->frameQueue.acquirePop();
        if (!packet) {
            usleep(1000);
            continue;
        }
        defer(pipeline->frameQueue.commitPop());
        FLOGI("VIDEO PKT OUT pts:%llums, in PKT caches:%llu", packet->ptsUs/1000, pipeline->frameQueue.size());
        if (droppingPackets && packet->frameFlags == 0) {
            LOGW("Dropping packet pts=%lld", static_cast<long long>(packet->ptsUs));
            continue;
        }
        droppingPackets = false;
        const auto cached_packet_millis =  pipeline->frameQueue.cached_millis();
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
        enqueueDecoder(decoder, packet);
    }

    if (pipeline->decoder) {
        AMediaCodec_stop(pipeline->decoder.get());
        pipeline->decoder = nullptr;   
    }
    LOGD("Video decode thread exiting");
    return nullptr;
}

std::shared_ptr<VideoDecoderPipeline> videoDecoderCreate(ANativeWindow* surface, int codecType, int width, int height) {
    auto pipeline = std::make_shared<VideoDecoderPipeline>();
    pipeline->width = width;
    pipeline->height = height;
    pipeline->currentCodecType = (codecType == 6) ? "video/hevc" : "video/avc";

    if (surface) {
        pipeline->surface = surface;
    }

    LOGD("VideoDecoderPipeline created: %s %dx%d", pipeline->currentCodecType.c_str(), width, height);
    return pipeline;
}

int videoDecoderStart(std::shared_ptr<VideoDecoderPipeline> pipeline) {
    if (!pipeline) return -1;

    pipeline->running.store(true);
    pipeline->decodeThread = std::thread([pipeline]() { decodeThreadFunc(pipeline.get()); });
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
    if (!pipeline || !pipeline->running.load() || pipeline->destroyed.load() || size <= 0) return;
    Packet *slot = pipeline->frameQueue.acquirePush();
    if (!slot) {
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            slot = pipeline->frameQueue.acquirePush();
        } while (!slot);
    }
    slot->setData(data, static_cast<uint32_t>(size));
    slot->ptsUs = ptsUs;
    slot->frameFlags = frameFlags;
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
            LOGD("VIDEO PKG IN pts:%llums, in packet caches:%llu/%llums", ptsUs/1000, pipeline->frameQueue.size(), (lastUs - firstUs)/1000);
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
