#include "easyrtc_media.h"
#include "EasyRTCAPI.h"
#include <cstring>
#include <string>

void releaseCaptureSession(MediaPipeline* pipeline) {
    if (pipeline->captureSession) {
        ACameraCaptureSession_stopRepeating(pipeline->captureSession);
        ACameraCaptureSession_close(pipeline->captureSession);
        pipeline->captureSession = nullptr;
    }
    if (pipeline->captureRequest) {
        ACaptureRequest_free(pipeline->captureRequest);
        pipeline->captureRequest = nullptr;
    }
    if (pipeline->encoderTarget) {
        ACameraOutputTarget_free(pipeline->encoderTarget);
        pipeline->encoderTarget = nullptr;
    }
    if (pipeline->previewTarget) {
        ACameraOutputTarget_free(pipeline->previewTarget);
        pipeline->previewTarget = nullptr;
    }
    if (pipeline->encoderOutput) {
        ACaptureSessionOutput_free(pipeline->encoderOutput);
        pipeline->encoderOutput = nullptr;
    }
    if (pipeline->previewOutput) {
        ACaptureSessionOutput_free(pipeline->previewOutput);
        pipeline->previewOutput = nullptr;
    }
    if (pipeline->outputContainer) {
        ACaptureSessionOutputContainer_free(pipeline->outputContainer);
        pipeline->outputContainer = nullptr;
    }
}

bool startCameraCapture(MediaPipeline* pipeline) {
    if (!pipeline->cameraDevice || !pipeline->window) {
        LOGE("startCameraCapture: cameraDevice or window is null");
        return false;
    }

    pthread_mutex_lock(&pipeline->mutex);
    releaseCaptureSession(pipeline);

    camera_status_t camStatus;

    camStatus = ACaptureSessionOutputContainer_create(&pipeline->outputContainer);
    if (camStatus != ACAMERA_OK) {
        LOGE("Failed to create output container: %d", camStatus);
        releaseCaptureSession(pipeline);
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    camStatus = ACaptureSessionOutput_create(reinterpret_cast<ACameraWindowType*>(pipeline->window),
                                              &pipeline->encoderOutput);
    if (camStatus != ACAMERA_OK) {
        LOGE("Failed to create encoder output: %d", camStatus);
        releaseCaptureSession(pipeline);
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    camStatus = ACaptureSessionOutputContainer_add(pipeline->outputContainer, pipeline->encoderOutput);
    if (camStatus != ACAMERA_OK) {
        LOGE("Failed to add encoder output to container: %d", camStatus);
        releaseCaptureSession(pipeline);
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    camStatus = ACameraDevice_createCaptureRequest(pipeline->cameraDevice, TEMPLATE_RECORD,
                                                    &pipeline->captureRequest);
    if (camStatus != ACAMERA_OK) {
        LOGE("Failed to create capture request: %d", camStatus);
        releaseCaptureSession(pipeline);
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    camStatus = ACameraOutputTarget_create(reinterpret_cast<ACameraWindowType*>(pipeline->window),
                                            &pipeline->encoderTarget);
    if (camStatus != ACAMERA_OK) {
        LOGE("Failed to create encoder target: %d", camStatus);
        releaseCaptureSession(pipeline);
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    camStatus = ACaptureRequest_addTarget(pipeline->captureRequest, pipeline->encoderTarget);
    if (camStatus != ACAMERA_OK) {
        LOGE("Failed to add encoder target to request: %d", camStatus);
        releaseCaptureSession(pipeline);
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    if (pipeline->previewWindow) {
        camStatus = ACaptureSessionOutput_create(reinterpret_cast<ACameraWindowType*>(pipeline->previewWindow),
                                                  &pipeline->previewOutput);
        if (camStatus != ACAMERA_OK) {
            LOGE("Failed to create preview output: %d", camStatus);
            releaseCaptureSession(pipeline);
            pthread_mutex_unlock(&pipeline->mutex);
            return false;
        }

        camStatus = ACaptureSessionOutputContainer_add(pipeline->outputContainer, pipeline->previewOutput);
        if (camStatus != ACAMERA_OK) {
            LOGE("Failed to add preview output to container: %d", camStatus);
            releaseCaptureSession(pipeline);
            pthread_mutex_unlock(&pipeline->mutex);
            return false;
        }

        camStatus = ACameraOutputTarget_create(reinterpret_cast<ACameraWindowType*>(pipeline->previewWindow),
                                                &pipeline->previewTarget);
        if (camStatus != ACAMERA_OK) {
            LOGE("Failed to create preview target: %d", camStatus);
            releaseCaptureSession(pipeline);
            pthread_mutex_unlock(&pipeline->mutex);
            return false;
        }

        camStatus = ACaptureRequest_addTarget(pipeline->captureRequest, pipeline->previewTarget);
        if (camStatus != ACAMERA_OK) {
            LOGE("Failed to add preview target to request: %d", camStatus);
            releaseCaptureSession(pipeline);
            pthread_mutex_unlock(&pipeline->mutex);
            return false;
        }
    }

    uint8_t afMode = ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    ACaptureRequest_setEntry_u8(pipeline->captureRequest,
                                 ACAMERA_CONTROL_AF_MODE, 1, &afMode);

    static const ACameraCaptureSession_stateCallbacks sessionCallbacks = {
        nullptr, nullptr, nullptr, nullptr
    };

    camStatus = ACameraDevice_createCaptureSession(pipeline->cameraDevice, pipeline->outputContainer,
                                                    &sessionCallbacks, &pipeline->captureSession);
    if (camStatus != ACAMERA_OK) {
        LOGE("Failed to create capture session: %d", camStatus);
        releaseCaptureSession(pipeline);
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    camStatus = ACameraCaptureSession_setRepeatingRequest(pipeline->captureSession, nullptr, 1,
                                                          &pipeline->captureRequest, nullptr);
    if (camStatus != ACAMERA_OK) {
        LOGE("Failed to set repeating request: %d", camStatus);
        releaseCaptureSession(pipeline);
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    LOGD("Camera capture session started");
    pthread_mutex_unlock(&pipeline->mutex);
    return true;
}

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

static void onCameraDeviceDisconnected(void* context, ACameraDevice* device) {
    LOGD("Camera device disconnected");
    auto* pipeline = static_cast<MediaPipeline*>(context);
    if (pipeline) {
        pthread_mutex_lock(&pipeline->mutex);
        pipeline->cameraDevice = nullptr;
        releaseCaptureSession(pipeline);
        pthread_mutex_unlock(&pipeline->mutex);
    }
}

static void onCameraDeviceError(void* context, ACameraDevice* device, int error) {
    LOGE("Camera device error: %d", error);
    auto* pipeline = static_cast<MediaPipeline*>(context);
    if (pipeline) {
        pthread_mutex_lock(&pipeline->mutex);
        pipeline->cameraDevice = nullptr;
        releaseCaptureSession(pipeline);
        pthread_mutex_unlock(&pipeline->mutex);
    }
}

static ACameraDevice_StateCallbacks getCameraDeviceCallbacks(void* ctx) {
    ACameraDevice_StateCallbacks cb{};
    cb.context = ctx;
    cb.onDisconnected = onCameraDeviceDisconnected;
    cb.onError = onCameraDeviceError;
    return cb;
}

void closeCamera(MediaPipeline* pipeline) {
    pthread_mutex_lock(&pipeline->mutex);
    releaseCaptureSession(pipeline);
    if (pipeline->cameraDevice) {
        ACameraDevice_close(pipeline->cameraDevice);
        pipeline->cameraDevice = nullptr;
    }
    pthread_mutex_unlock(&pipeline->mutex);
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
            pthread_mutex_lock(&pipeline->mutex);
            delete[] pipeline->sps_pps_buffer;
            pipeline->sps_pps_buffer = new uint8_t[dataSize];
            memcpy(pipeline->sps_pps_buffer, data, dataSize);
            pipeline->sps_pps_size = dataSize;
            pthread_mutex_unlock(&pipeline->mutex);
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
            pthread_mutex_lock(&pipeline->mutex);
            if (pipeline->sps_pps_buffer && pipeline->sps_pps_size > 0) {
                size_t totalSize = pipeline->sps_pps_size + dataSize;
                auto* combined = new uint8_t[totalSize];
                memcpy(combined, pipeline->sps_pps_buffer, pipeline->sps_pps_size);
                memcpy(combined + pipeline->sps_pps_size, data, dataSize);
                frame.frameData = combined;
                frame.size = static_cast<UINT32>(totalSize);
            }
            pthread_mutex_unlock(&pipeline->mutex);
        }

        if (!pipeline->transceiver) {
            AMediaCodec_releaseOutputBuffer(pipeline->encoder, bufIdx, false);
            continue;
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


