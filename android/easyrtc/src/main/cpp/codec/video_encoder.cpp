#include "codec/video_encoder.h"
#include "EasyRTCAPI.h"
#include "session/common.h"
#include "session/media_session.h"
#include "session/camera_error_handler.h"
#include <cassert>
#include <cstring>
#include <string>
#include <sstream>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadataTags.h>
#include <media/NdkMediaCodec.h>
#include <chrono>
#include "utils/defer.hpp"

std::string findCameraId(int facing) {
    ACameraManager* mgr = ACameraManager_create();
    if (!mgr) {
        LOGE("Failed to create ACameraManager");
        return "";
    }

    ACameraIdList* idList = nullptr;
    ACameraManager_getCameraIdList(mgr, &idList);

    std::string result;
    if (idList) {
        for (int i = 0; i < idList->numCameras; i++) {
            const char* id = idList->cameraIds[i];
            ACameraMetadata* metadata = nullptr;
            ACameraManager_getCameraCharacteristics(mgr, id, &metadata);
            if (!metadata) continue;

            ACameraMetadata_const_entry entry;
            if (ACameraMetadata_getConstEntry(metadata, ACAMERA_LENS_FACING, &entry) == ACAMERA_OK) {
                int32_t lensFacing = static_cast<int32_t>(entry.data.u8[0]);
                if ((facing == 0 && lensFacing == ACAMERA_LENS_FACING_BACK) ||
                    (facing == 1 && lensFacing == ACAMERA_LENS_FACING_FRONT)) {
                    result = id;
                    ACameraMetadata_free(metadata);
                    break;
                }
            }
            ACameraMetadata_free(metadata);
        }
        ACameraManager_deleteCameraIdList(idList);
    }
    ACameraManager_delete(mgr);
    return result;
}

bool MediaPipeline::initEncoder(MediaSession *session)
{
    LOGI("[CRITICAL] initEncoder: mime=%s %dx%d @ %dbps fps=%d",
         mime.c_str(), width, height, bitrate, fps);

    auto tryCreate = [this](const std::string &name) -> AMediaCodec* {
        if (name.empty()) {
            LOGI("[CRITICAL] initEncoder: trying createEncoderByType(%s)", mime.c_str());
            return AMediaCodec_createEncoderByType(mime.c_str());
        }
        LOGI("[CRITICAL] initEncoder: trying createCodecByName(%s)", name.c_str());
        return AMediaCodec_createCodecByName(name.c_str());
    };

    auto tryConfigure = [this](AMediaCodec *encoder) -> bool {
        if (!encoder) {
            LOGE("[CRITICAL] initEncoder: createCodec FAILED");
            return false;
        }
        assert(format);
        media_status_t status = AMediaCodec_configure(encoder, format.get(), nullptr, nullptr,
                AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
        if (status != AMEDIA_OK) {
            LOGE("[CRITICAL] initEncoder: configure FAILED status=%d", status);
            AMediaCodec_delete(encoder);
            return false;
        }
        ANativeWindow* inputWindow = nullptr;
        media_status_t surfStatus = AMediaCodec_createInputSurface(encoder, &inputWindow);
        if (surfStatus != AMEDIA_OK || !inputWindow) {
            LOGW("[CRITICAL] initEncoder: createInputSurface FAILED status=%d", surfStatus);
            AMediaCodec_delete(encoder);
            return false;
        }
        this->encoderInputSurface = std::shared_ptr<ANativeWindow>(inputWindow, ANativeWindow_release);
        this->encoder = encoder;
        LOGI("[CRITICAL] initEncoder: SUCCESS encoder=%p window=%p", encoder, inputWindow);
        return true;
    };

    if (tryConfigure(tryCreate(""))) return true;

    if (session) {
        std::string candidates = queryEncoderForSurfaceInput(session, mime);
        if (!candidates.empty()) {
            LOGI("[CRITICAL] initEncoder: candidates for mime=%s: %s", mime.c_str(), candidates.c_str());
            std::istringstream iss(candidates);
            std::string name;
            while (std::getline(iss, name, ';')) {
                if (name.empty()) continue;
                if (tryConfigure(tryCreate(name))) return true;
            }
        }
    }

    LOGE("[CRITICAL] initEncoder: all attempts failed for mime=%s", mime.c_str());
    return false;
}

void MediaPipeline::startEncoder(MediaSession *session)
{
    running.store(true);
    outputThread = std::thread([](void *sessionPtr) {
        auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->videoEncoder);
    auto* pipeline = session->videoEncoder.get();
    LOGI("[OV] Output thread started");

    while (pipeline->running.load() && !session->shuttingDown.load()) {
        if (!pipeline->encoder) {
            break;
        }

        if (pipeline->requestKeyFramePending.exchange(false)) {
            AMediaFormat* params = AMediaFormat_new();
            AMediaFormat_setInt32(params, "request-sync", 0);
            media_status_t requestStatus = AMediaCodec_setParameters(pipeline->encoder, params);
            AMediaFormat_delete(params);
            if (requestStatus != AMEDIA_OK) {
                LOGW("[OV] request-sync failed: status=%d", requestStatus);
            } else {
                LOGI("[OV] request-sync applied on output thread");
            }
        }

        AMediaCodecBufferInfo info;
        ssize_t bufIdx = AMediaCodec_dequeueOutputBuffer(pipeline->encoder, &info, 10000);
        if (bufIdx < 0) {
            continue;
        }

        size_t outSize = 0;
        uint8_t* outBuf = AMediaCodec_getOutputBuffer(pipeline->encoder, bufIdx, &outSize);
        if (!outBuf) {
            assert(false && "Failed to get output buffer");
            AMediaCodec_releaseOutputBuffer(pipeline->encoder, bufIdx, false);
            continue;
        }

        uint8_t* data = outBuf + info.offset;
        size_t dataSize = static_cast<size_t>(info.size);

        if (info.flags & BUFFER_FLAG_CODEC_CONFIG) {
            pipeline->sps_pps_buffer.resize(dataSize);
            pipeline->sps_pps_buffer.assign(data, data + dataSize);
            pipeline->sps_pps_size = dataSize;
            AMediaCodec_releaseOutputBuffer(pipeline->encoder, bufIdx, false);
            continue;
        }

        EasyRTC_Frame frame{};
        frame.version = 0;
        frame.size = static_cast<UINT32>(dataSize);
        frame.flags = (info.flags & 1)
                ? EASYRTC_FRAME_FLAG_KEY_FRAME
                : EASYRTC_FRAME_FLAG_NONE;
        // MediaCodec output timestamp is in microseconds; convert to 100ns ticks.
        frame.presentationTs = static_cast<UINT64>(info.presentationTimeUs) * 10ULL;
        frame.decodingTs = frame.presentationTs;
        frame.frameData = data;
        frame.trackId = session->videoTrackId;

        const bool isConnected = (session->connectState == 3);

        if (frame.flags == EASYRTC_FRAME_FLAG_KEY_FRAME) {
            LOGI("[OV] Output key frame: size=%u, pts=%llu, preallocated buf=%zu", frame.size, static_cast<unsigned long long>(frame.presentationTs), pipeline->sps_pps_buffer.size());
            if (pipeline->sps_pps_size > 0) {
                pipeline->sps_pps_buffer.resize(pipeline->sps_pps_size);
                // let's expand the sps_pps_buffer, and append the current frame data after it, then send the combined buffer to the peer, so that the decoder on the peer side can get the sps/pps before decoding the key frame
                pipeline->sps_pps_buffer.insert(pipeline->sps_pps_buffer.end(), data, data + dataSize);
                assert(pipeline->sps_pps_buffer.size() == pipeline->sps_pps_size + dataSize);
                frame.frameData = pipeline->sps_pps_buffer.data();
                frame.size = static_cast<UINT32>(pipeline->sps_pps_buffer.size());
            }
        }

        // Discard non-key media frames until the connection is ready.
        if (!isConnected) {
            AMediaCodec_releaseOutputBuffer(pipeline->encoder, bufIdx, false);
            continue;
        }

        if (!pipeline->transceiver) {
            assert(false && "No transceiver available for sending frames");
            AMediaCodec_releaseOutputBuffer(pipeline->encoder, bufIdx, false);
            continue;
        }

        // LOGD("Sending frame: transceiver=%p size=%u flags=%u pts=%llu", pipeline->transceiver, frame.size, frame.flags, static_cast<unsigned long long>(frame.presentationTs));
        int result = 0;
        {
            auto begin_ = std::chrono::steady_clock::now();
            defer({
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - begin_).count();
                    if (duration > 10) {
                        LOGW("[OV] EasyRTC_SendFrame took %lld ms", static_cast<long long>(duration));
                    }
                });
            result = EasyRTC_SendFrame(pipeline->transceiver, &frame);
            if (result != 0) {
                LOGE("[OV] EasyRTC_SendFrame failed: %d, size=%u, flags=%u", result, frame.size, frame.flags);
            }else {
                session->videoFramesSent.fetch_add(1, std::memory_order_relaxed);
                FLOGI("[OV] EasyRTC_SendFrame success: size=%u, flags=%u, pts=%llu", frame.size, frame.flags, static_cast<unsigned long long>(frame.presentationTs));
            }
        }
        AMediaCodec_releaseOutputBuffer(pipeline->encoder, bufIdx, false);
    }

    LOGI("[OV] Output thread exiting");
    }, session);
}