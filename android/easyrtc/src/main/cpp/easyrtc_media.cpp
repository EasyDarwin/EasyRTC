#include "easyrtc_media.h"
#include "EasyRTCAPI.h"
#include <jni.h>
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

extern "C" {

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaPipeline_nativeCreate(
    JNIEnv* env, jobject thiz,
    jlong transceiverHandle, jint codec, jint width, jint height,
    jint bitrate, jint fps, jint iframeInterval) {
    auto* pipeline = new MediaPipeline();
    pipeline->transceiver = reinterpret_cast<EasyRTC_Transceiver>(transceiverHandle);
    pipeline->width = width;
    pipeline->height = height;
    pipeline->bitrate = bitrate;
    pipeline->fps = fps;
    pipeline->iframeInterval = iframeInterval;

    const char* mime = (codec == 6) ? "video/hevc" : "video/avc";
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, iframeInterval);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatSurface);
    pipeline->format = format;

    AMediaCodec* encoder = AMediaCodec_createEncoderByType(mime);
    if (!encoder) {
        LOGE("Failed to create encoder for mime: %s", mime);
        AMediaFormat_delete(format);
        delete pipeline;
        return 0;
    }

    media_status_t status = AMediaCodec_configure(encoder, format, nullptr, nullptr,
                                                   AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    if (status != AMEDIA_OK) {
        LOGE("Failed to configure encoder: %d", status);
        AMediaCodec_delete(encoder);
        AMediaFormat_delete(format);
        delete pipeline;
        return 0;
    }

    pipeline->encoder = encoder;
    LOGD("MediaPipeline created: %dx%d @ %d bps, mime=%s", width, height, bitrate, mime);
    return reinterpret_cast<jlong>(pipeline);
}

JNIEXPORT jobject JNICALL
Java_cn_easyrtc_media_MediaPipeline_nativeSetSurface(
    JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<MediaPipeline*>(pipelinePtr);
    if (!pipeline || !pipeline->encoder) {
        LOGE("nativeSetSurface: invalid pipeline or encoder");
        return nullptr;
    }

    ANativeWindow* window = nullptr;
    media_status_t status = AMediaCodec_createInputSurface(pipeline->encoder, &window);
    if (status != AMEDIA_OK || !window) {
        LOGE("Failed to create input surface: %d", status);
        return nullptr;
    }

    pipeline->window = window;
    jobject surface = ANativeWindow_toSurface(env, window);
    LOGD("Input surface created");
    return surface;
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaPipeline_nativeStart(
    JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<MediaPipeline*>(pipelinePtr);
    if (!pipeline || !pipeline->encoder) {
        LOGE("nativeStart: invalid pipeline or encoder");
        return -1;
    }

    media_status_t status = AMediaCodec_start(pipeline->encoder);
    if (status != AMEDIA_OK) {
        LOGE("Failed to start encoder: %d", status);
        return -1;
    }

    ACameraManager* cameraMgr = ACameraManager_create();
    if (cameraMgr) {
        std::string cameraId = findCameraId(pipeline->cameraFacing);
        if (!cameraId.empty()) {
            ACameraDevice_StateCallbacks cb = getCameraDeviceCallbacks(pipeline);
            ACameraDevice* device = nullptr;
            camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb, &device);
            if (camStatus != ACAMERA_OK || !device) {
                LOGE("Failed to open camera %s: %d", cameraId.c_str(), camStatus);
            } else {
                pipeline->cameraDevice = device;
                LOGD("Camera %s opened, facing=%d", cameraId.c_str(), pipeline->cameraFacing);
                startCameraCapture(pipeline);
            }
        } else {
            LOGE("No camera found for facing=%d", pipeline->cameraFacing);
        }
        ACameraManager_delete(cameraMgr);
    }

    pipeline->running.store(true);
    int ret = pthread_create(&pipeline->output_thread, nullptr, outputThreadFunc, pipeline);
    if (ret != 0) {
        LOGE("Failed to create output thread: %d", ret);
        pipeline->running.store(false);
        closeCamera(pipeline);
        AMediaCodec_stop(pipeline->encoder);
        return -1;
    }

    LOGD("Encoder started, output thread spawned");
    return 0;
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaPipeline_nativeStop(
    JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<MediaPipeline*>(pipelinePtr);
    if (!pipeline) return;

    if (pipeline->running.exchange(false)) {
        closeCamera(pipeline);
        if (pipeline->encoder) {
            AMediaCodec_signalEndOfInputStream(pipeline->encoder);
        }
        if (pipeline->output_thread) {
            pthread_join(pipeline->output_thread, nullptr);
            pipeline->output_thread = 0;
        }
        if (pipeline->encoder) {
            AMediaCodec_stop(pipeline->encoder);
        }
        LOGD("Encoder stopped");
    }
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaPipeline_nativeRequestKeyFrame(
    JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<MediaPipeline*>(pipelinePtr);
    if (!pipeline || !pipeline->encoder) return;

    AMediaFormat* params = AMediaFormat_new();
    AMediaFormat_setInt32(params, "request-sync", 0);
    AMediaCodec_setParameters(pipeline->encoder, params);
    AMediaFormat_delete(params);
    LOGD("Key frame requested");
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaPipeline_nativeSetTransceiver(
    JNIEnv* env, jobject thiz, jlong pipelinePtr, jlong transceiverHandle) {
    auto* pipeline = reinterpret_cast<MediaPipeline*>(pipelinePtr);
    if (!pipeline) return;
    pipeline->transceiver = reinterpret_cast<EasyRTC_Transceiver>(transceiverHandle);
    LOGD("Transceiver updated: %p", pipeline->transceiver);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaPipeline_nativeSetPreviewSurface(
    JNIEnv* env, jobject thiz, jlong pipelinePtr, jobject surface) {
    auto* pipeline = reinterpret_cast<MediaPipeline*>(pipelinePtr);
    if (!pipeline) return;

    if (pipeline->previewWindow) {
        ANativeWindow_release(pipeline->previewWindow);
        pipeline->previewWindow = nullptr;
    }

    if (surface) {
        pipeline->previewWindow = ANativeWindow_fromSurface(env, surface);
        LOGD("Preview surface set");
    } else {
        LOGD("Preview surface cleared");
    }
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaPipeline_nativeSwitchCamera(
    JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<MediaPipeline*>(pipelinePtr);
    if (!pipeline) return;
    if (!pipeline->running.load()) {
        LOGE("nativeSwitchCamera: pipeline not running");
        return;
    }

    pthread_mutex_lock(&pipeline->mutex);
    closeCamera(pipeline);

    pipeline->cameraFacing = (pipeline->cameraFacing == 0) ? 1 : 0;

    ACameraManager* cameraMgr = ACameraManager_create();
    if (cameraMgr) {
        std::string cameraId = findCameraId(pipeline->cameraFacing);
        if (!cameraId.empty()) {
            ACameraDevice_StateCallbacks cb = getCameraDeviceCallbacks(pipeline);
            ACameraDevice* device = nullptr;
            camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb, &device);
            if (camStatus != ACAMERA_OK || !device) {
                LOGE("Failed to switch camera %s: %d", cameraId.c_str(), camStatus);
            } else {
                pipeline->cameraDevice = device;
                LOGD("Switched to camera %s, facing=%d", cameraId.c_str(), pipeline->cameraFacing);
                startCameraCapture(pipeline);
            }
        } else {
            LOGE("No camera found for facing=%d", pipeline->cameraFacing);
        }
        ACameraManager_delete(cameraMgr);
    }
    pthread_mutex_unlock(&pipeline->mutex);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaPipeline_nativeRelease(
    JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<MediaPipeline*>(pipelinePtr);
    if (!pipeline) return;

    Java_cn_easyrtc_media_MediaPipeline_nativeStop(env, thiz, pipelinePtr);

    closeCamera(pipeline);

    if (pipeline->encoder) {
        AMediaCodec_delete(pipeline->encoder);
        pipeline->encoder = nullptr;
    }
    if (pipeline->format) {
        AMediaFormat_delete(pipeline->format);
        pipeline->format = nullptr;
    }
    if (pipeline->window) {
        ANativeWindow_release(pipeline->window);
        pipeline->window = nullptr;
    }
    if (pipeline->previewWindow) {
        ANativeWindow_release(pipeline->previewWindow);
        pipeline->previewWindow = nullptr;
    }
    {
        pthread_mutex_lock(&pipeline->mutex);
        delete[] pipeline->sps_pps_buffer;
        pipeline->sps_pps_buffer = nullptr;
        pipeline->sps_pps_size = 0;
        pthread_mutex_unlock(&pipeline->mutex);
    }
    pthread_mutex_destroy(&pipeline->mutex);
    delete pipeline;
    LOGD("MediaPipeline released");
}

}
