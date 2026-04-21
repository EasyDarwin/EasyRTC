#include "easyrtc_media.h"
#include "EasyRTCAPI.h"
#include <cstring>
#include <string>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadataTags.h>

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

void* outputThreadFunc(void* arg) {
    auto* pipeline = static_cast<MediaPipeline*>(arg);
    LOGD("Output thread started");

    while (pipeline->running.load()) {
        if (!pipeline->encoder) {
            break;
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
            std::lock_guard<std::recursive_mutex> lock(pipeline->mutex);
            delete[] pipeline->sps_pps_buffer;
            pipeline->sps_pps_buffer = new uint8_t[dataSize];
            memcpy(pipeline->sps_pps_buffer, data, dataSize);
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
            std::lock_guard<std::recursive_mutex> lock(pipeline->mutex);
            if (pipeline->sps_pps_buffer && pipeline->sps_pps_size > 0) {
                size_t totalSize = pipeline->sps_pps_size + dataSize;
                auto* combined = new uint8_t[totalSize];
                memcpy(combined, pipeline->sps_pps_buffer, pipeline->sps_pps_size);
                memcpy(combined + pipeline->sps_pps_size, data, dataSize);
                frame.frameData = combined;
                frame.size = static_cast<UINT32>(totalSize);
            }
        }

        if (!pipeline->transceiver) {
            AMediaCodec_releaseOutputBuffer(pipeline->encoder, bufIdx, false);
            continue;
        }

        LOGD("Sending frame: transceiver=%p size=%u flags=%u pts=%llu", pipeline->transceiver, frame.size, frame.flags, static_cast<unsigned long long>(frame.presentationTs));
        int sendResult = EasyRTC_SendFrame(pipeline->transceiver, &frame);
        if (sendResult != 0) {
            LOGE("EasyRTC_SendFrame failed: %d, size=%u, flags=%u", sendResult, frame.size, frame.flags);
        }

        if (frame.frameData != data) {
            delete[] frame.frameData;
        }

        AMediaCodec_releaseOutputBuffer(pipeline->encoder, bufIdx, false);
    }

    LOGD("Output thread exiting");
    return nullptr;
}
