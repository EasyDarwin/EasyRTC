#include "easyrtc_media.h"
#include "EasyRTCAPI.h"
#include "easyrtc_common.h"
#include "easyrtc_session.h"
#include <cassert>
#include <cstring>
#include <string>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadataTags.h>
#include <media/NdkMediaCodec.h>

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

bool MediaPipeline::initEncoder()
{
    LOGI("[CRITICAL] initEncoder: mime=%s %dx%d @ %dbps fps=%d",
         mime.c_str(), width, height, bitrate, fps);
    AMediaCodec* encoder = AMediaCodec_createEncoderByType(mime.c_str());
    if (!encoder) {
        LOGE("[CRITICAL] initEncoder: createEncoderByType FAILED for %s", mime.c_str());
        assert(false);
        return false;
    }
    assert(format);
    media_status_t status = AMediaCodec_configure(encoder, format.get(), nullptr, nullptr,
            AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    if (status != AMEDIA_OK) {
        LOGE("[CRITICAL] initEncoder: configure FAILED status=%d", status);
        AMediaCodec_delete(encoder);
        assert(false);
        return false;
    }


    ANativeWindow* inputWindow = nullptr;
    media_status_t surfStatus = AMediaCodec_createInputSurface(encoder, &inputWindow);
    if (surfStatus != AMEDIA_OK || !inputWindow) {
        LOGE("[CRITICAL] initEncoder: createInputSurface FAILED status=%d", surfStatus);
        AMediaCodec_delete(encoder);
        assert(false);
        return false;
    }
    this->window = inputWindow;
    this->encoder = encoder;
    LOGI("[CRITICAL] initEncoder: SUCCESS encoder=%p window=%p", encoder, inputWindow);
    return true;
}

void* outputThreadFunc(void* arg) {
    auto *session = reinterpret_cast<MediaSession *>(arg);
    assert(session && "Invalid session");
    assert(session->videoEncoder);
    auto* pipeline = session->videoEncoder.get();
    LOGI("[VO] Output thread started");

    while (pipeline->running.load()) {
        if (!pipeline->encoder) {
            break;
        }
        if (session->connectState != 3) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        AMediaCodecBufferInfo info;
        ssize_t bufIdx = AMediaCodec_dequeueOutputBuffer(pipeline->encoder, &info, 10000);
        if (bufIdx < 0) {
            continue;
        }

        size_t outSize = 0;
        uint8_t* outBuf = AMediaCodec_getOutputBuffer(pipeline->encoder, bufIdx, &outSize);
        if (!outBuf) {
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
        frame.presentationTs = static_cast<UINT64>(info.presentationTimeUs) * 100ULL;
        frame.decodingTs = frame.presentationTs;
        frame.frameData = data;
        frame.trackId = 0;

        if (frame.flags == EASYRTC_FRAME_FLAG_KEY_FRAME) {
            LOGI("[VO] Output key frame: size=%u, pts=%llu, sps_pps_size=%zu", frame.size, static_cast<unsigned long long>(frame.presentationTs), pipeline->sps_pps_buffer.size());
            if (pipeline->sps_pps_size > 0) {
                pipeline->sps_pps_buffer.resize(pipeline->sps_pps_size);
                // let's expand the sps_pps_buffer, and append the current frame data after it, then send the combined buffer to the peer, so that the decoder on the peer side can get the sps/pps before decoding the key frame
                pipeline->sps_pps_buffer.insert(pipeline->sps_pps_buffer.end(), data, data + dataSize);
                assert(pipeline->sps_pps_buffer.size() == pipeline->sps_pps_size + dataSize);
                frame.frameData = pipeline->sps_pps_buffer.data();
                frame.size = static_cast<UINT32>(pipeline->sps_pps_buffer.size());
            }
        }

        if (!pipeline->transceiver) {
            AMediaCodec_releaseOutputBuffer(pipeline->encoder, bufIdx, false);
            continue;
        }

        // LOGD("Sending frame: transceiver=%p size=%u flags=%u pts=%llu", pipeline->transceiver, frame.size, frame.flags, static_cast<unsigned long long>(frame.presentationTs));
        int sendResult = EasyRTC_SendFrame(pipeline->transceiver, &frame);
        if (sendResult != 0) {
            LOGE("[VO] EasyRTC_SendFrame failed: %d, size=%u, flags=%u", sendResult, frame.size, frame.flags);
        }else {
            FLOGI("[VO] EasyRTC_SendFrame success: size=%u, flags=%u, pts=%llu", frame.size, frame.flags, static_cast<unsigned long long>(frame.presentationTs));
        }

        AMediaCodec_releaseOutputBuffer(pipeline->encoder, bufIdx, false);
    }

    LOGI("[VO] Output thread exiting");
    return nullptr;
}
