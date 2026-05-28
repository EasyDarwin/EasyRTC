#include "session/media_session.h"
#include "session/common.h"
#include "codec/video_encoder.h"
#include "capture/audio_capture.h"
#include "codec/audio_playback.h"
#include "codec/video_decoder.h"
#include "util/frame_dump.h"
#include "gl/encoder_gl_bridge.h"
#include <android/native_window.h>
#include "utils/defer.hpp"
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadataTags.h>
#include <android/surface_texture_jni.h>
#include <cassert>
#include <chrono>
#include <media/NdkMediaFormat.h>
#include <unistd.h>
#include <jni.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>

namespace {
    // std::atomic<uint64_t> gVideoCbCount{0};
    // std::atomic<uint64_t> gVideoCbBytes{0};
    // std::atomic<uint64_t> gVideoCbAnnexB{0};
    // std::atomic<uint64_t> gVideoCbAvcc{0};

}

static void notifyInputKbpsStats(MediaSession *session,
                                 double videoKbps,
                                 double audioKbps,
                                 double totalKbps) {
    if (!session || !session->jvm || !session->javaObj) {
        return;
    }
    auto _begin = std::chrono::steady_clock::now();
    defer({
        auto _duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - _begin).count();
        if (_duration > 10) {
            LOGW("notifyInputKbpsStats took %lld ms", static_cast<long long>(_duration));
        }
    });
    JNIEnv *env = nullptr;
    bool attached = false;
    int ret = session->jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    if (ret != JNI_OK) {
        if (session->jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            attached = true;
        } else {
            LOGW("notifyInputKbpsStats: failed to attach thread to JVM");
            return;
        }
    }

    jclass clazz = env->GetObjectClass(session->javaObj);
    if (clazz) {
        jmethodID mid = env->GetMethodID(clazz, "onInputKbpsStats", "(DDD)V");
        if (mid) {
            env->CallVoidMethod(session->javaObj, mid,
                                static_cast<jdouble>(videoKbps),
                                static_cast<jdouble>(audioKbps),
                                static_cast<jdouble>(totalKbps));
        }
    }

    if (attached) {
        session->jvm->DetachCurrentThread();
    }
}

static void accumulateMediaInputBytes(MediaSession *session, uint32_t videoBytes, uint32_t audioBytes) {
    if (!session) {
        return;
    }
    if (videoBytes > 0) {
        session->mediaInputVideoBytesWindow.fetch_add(videoBytes, std::memory_order_relaxed);
    }
    if (audioBytes > 0) {
        session->mediaInputAudioBytesWindow.fetch_add(audioBytes, std::memory_order_relaxed);
    }
}

static void reportMediaInputKbpsStats(MediaSession *session) {
    if (!session) {
        return;
    }

    const int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t lastNs = session->mediaInputLastReportNs.load(std::memory_order_relaxed);
    if (lastNs == 0) {
        session->mediaInputLastReportNs.compare_exchange_strong(lastNs, nowNs, std::memory_order_relaxed);
        return;
    }
    const int64_t durationNs = nowNs - lastNs;
    if (durationNs <= 0) {
        return;
    }
    if (!session->mediaInputLastReportNs.compare_exchange_strong(lastNs, nowNs, std::memory_order_relaxed)) {
        return;
    }

    const uint64_t videoWindowBytes = session->mediaInputVideoBytesWindow.exchange(0, std::memory_order_relaxed);
    const uint64_t audioWindowBytes = session->mediaInputAudioBytesWindow.exchange(0, std::memory_order_relaxed);
    const double elapsedSec = static_cast<double>(durationNs) / 1e9;
    if (elapsedSec <= 0.0) {
        return;
    }

    const double videoKbps = static_cast<double>(videoWindowBytes) * 8.0 / 1000.0 / elapsedSec;
    const double audioKbps = static_cast<double>(audioWindowBytes) * 8.0 / 1000.0 / elapsedSec;
    const double totalKbps = videoKbps + audioKbps;

    session->mediaInputVideoKbpsX100.store(static_cast<uint32_t>(videoKbps * 100.0), std::memory_order_relaxed);
    session->mediaInputAudioKbpsX100.store(static_cast<uint32_t>(audioKbps * 100.0), std::memory_order_relaxed);
    session->mediaInputTotalKbpsX100.store(static_cast<uint32_t>(totalKbps * 100.0), std::memory_order_relaxed);

    notifyInputKbpsStats(session, videoKbps, audioKbps, totalKbps);
}

static void releaseCaptureSessionOutputs(MediaSession *s) {
    if (s->captureRequest) {
        ACaptureRequest_free(s->captureRequest);
        s->captureRequest = nullptr;
    }
    if (s->previewTarget) {
        ACameraOutputTarget_free(s->previewTarget);
        s->previewTarget = nullptr;
    }
    if (s->cameraInputTarget) {
        ACameraOutputTarget_free(s->cameraInputTarget);
        s->cameraInputTarget = nullptr;
    }
    if (s->previewOutput) {
        ACaptureSessionOutput_free(s->previewOutput);
        s->previewOutput = nullptr;
    }
    if (s->cameraInputOutput) {
        ACaptureSessionOutput_free(s->cameraInputOutput);
        s->cameraInputOutput = nullptr;
    }
    if (s->outputContainer) {
        ACaptureSessionOutputContainer_free(s->outputContainer);
        s->outputContainer = nullptr;
    }
    s->captureSession = nullptr;
}

static void releaseCaptureSession(MediaSession *s) {
    LOGI("[CRITICAL] releaseCaptureSession ENTRY: s=%p", s);
    if (s->captureSession) {
        ACameraCaptureSession_stopRepeating(s->captureSession);
        ACameraCaptureSession_close(s->captureSession);
        s->captureSession = nullptr;
    }
    releaseCaptureSessionOutputs(s);
}

static void closeCamera(MediaSession *s) {
    LOGI("[CRITICAL] closeCamera ENTRY: s=%p", s);
    std::lock_guard<std::mutex> lock(s->cameraMutex);
    if (s->captureSession) {
        ACameraCaptureSession_stopRepeating(s->captureSession);
        s->captureSession = nullptr;
    }
    if (s->cameraDevice) {
        ACameraDevice_close(s->cameraDevice);
        s->cameraDevice = nullptr;
    }
    releaseCaptureSessionOutputs(s);
}

static bool createCaptureSession(MediaSession *s, bool withEncoder) {
    LOGI("[CRITICAL] createCaptureSession ENTRY: s=%p withEncoder=%d", s, withEncoder ? 1 : 0);
    camera_status_t camStatus;

    camStatus = ACaptureSessionOutputContainer_create(&s->outputContainer);
    if (camStatus != ACAMERA_OK) {
        releaseCaptureSession(s);
        return false;
    }

    if (s->cameraInputWindow) {
        camStatus = ACaptureSessionOutput_create(
                reinterpret_cast<ACameraWindowType *>(s->cameraInputWindow.get()), &s->cameraInputOutput);
        if (camStatus != ACAMERA_OK) {
            releaseCaptureSession(s);
            return false;
        }
        camStatus = ACaptureSessionOutputContainer_add(s->outputContainer, s->cameraInputOutput);
        if (camStatus != ACAMERA_OK) {
            releaseCaptureSession(s);
            return false;
        }
    } else if (withEncoder) {
        LOGE("[CRITICAL] EncoderRotate: cameraInputWindow missing; refuse direct camera->encoder path");
        releaseCaptureSession(s);
        return false;
    }

    if (s->previewWindow) {
        camStatus = ACaptureSessionOutput_create(
                reinterpret_cast<ACameraWindowType *>(s->previewWindow.get()), &s->previewOutput);
        if (camStatus != ACAMERA_OK) {
            releaseCaptureSession(s);
            return false;
        }
        camStatus = ACaptureSessionOutputContainer_add(s->outputContainer, s->previewOutput);
        if (camStatus != ACAMERA_OK) {
            releaseCaptureSession(s);
            return false;
        }
    }

    camStatus = ACameraDevice_createCaptureRequest(s->cameraDevice,
                                                   withEncoder ? TEMPLATE_RECORD : TEMPLATE_PREVIEW,
                                                   &s->captureRequest);
    if (camStatus != ACAMERA_OK) {
        releaseCaptureSession(s);
        return false;
    }

    if (withEncoder && s->cameraInputWindow) {
        camStatus = ACameraOutputTarget_create(
                reinterpret_cast<ACameraWindowType *>(s->cameraInputWindow.get()), &s->cameraInputTarget);
        if (camStatus != ACAMERA_OK) {
            releaseCaptureSession(s);
            return false;
        }
        camStatus = ACaptureRequest_addTarget(s->captureRequest, s->cameraInputTarget);
        if (camStatus != ACAMERA_OK) {
            releaseCaptureSession(s);
            return false;
        }
    } else if (withEncoder) {
        LOGE("[CRITICAL] EncoderRotate: cameraInputTarget missing; refuse direct camera->encoder path");
        releaseCaptureSession(s);
        return false;
    }

    if (s->previewWindow) {
        camStatus = ACameraOutputTarget_create(
                reinterpret_cast<ACameraWindowType *>(s->previewWindow.get()), &s->previewTarget);
        if (camStatus != ACAMERA_OK) {
            releaseCaptureSession(s);
            return false;
        }
        camStatus = ACaptureRequest_addTarget(s->captureRequest, s->previewTarget);
        if (camStatus != ACAMERA_OK) {
            releaseCaptureSession(s);
            return false;
        }
    }

    uint8_t afMode = ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    ACaptureRequest_setEntry_u8(s->captureRequest, ACAMERA_CONTROL_AF_MODE, 1, &afMode);

    static const ACameraCaptureSession_stateCallbacks sessionCallbacks = {nullptr, nullptr, nullptr,
                                                                          nullptr};
    camStatus = ACameraDevice_createCaptureSession(s->cameraDevice, s->outputContainer,
                                                   &sessionCallbacks, &s->captureSession);
    if (camStatus != ACAMERA_OK) {
        releaseCaptureSession(s);
        return false;
    }

    camStatus = ACameraCaptureSession_setRepeatingRequest(s->captureSession, nullptr, 1,
                                                          &s->captureRequest, nullptr);
    if (camStatus != ACAMERA_OK) {
        releaseCaptureSession(s);
        return false;
    }

    return true;
}

static void onRemoteVideoSizeCallback(void *userPtr, int width, int height);

static int mediaTransceiverCallback(void *userPtr,
                                    EASYRTC_TRANSCEIVER_CALLBACK_TYPE_E type,
                                    EasyRTC_CODEC codecID,
                                    EasyRTC_Frame *frame,
                                    double bandwidthEstimation) {

    auto *session = static_cast<MediaSession *>(userPtr);
    assert(session && "Invalid session in transceiver callback");
    auto begin_ = std::chrono::steady_clock::now();
    defer({
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - begin_).count();
        if (duration > 10) {
            LOGW("mediaTransceiverCallback type=%d codec=%d frameSize=%u took %lld ms",
                 type, codecID, frame ? frame->size : 0, static_cast<long long>(duration));
        }
    });
    switch (type) {
        case EASYRTC_TRANSCEIVER_CALLBACK_VIDEO_FRAME:
            assert(frame && "invalid frame in video callback");
            assert(frame->frameData && "invalid frame data in video callback");
            assert(frame->size >= 0 && "invalid frame size in video callback");
            if (session->last_video_frame_idx != 0) {
                if (session->last_video_frame_idx + 1 != frame->index) {
                    session->video_frame_loss_count += frame->index - session->last_video_frame_idx - 1;
                    LOGW("VIDEO_CB frame index jump: last=%u current=%u, lost=%u", session->last_video_frame_idx, frame->index, session->video_frame_loss_count);
                }
            }
            session->last_video_frame_idx = frame->index;
            if (frame->flags & EASYRTC_FRAME_FLAG_KEY_FRAME) {
                static bool dumped = false;
                if (!dumped) {
                    LOGI("dumping key frame");
                    dumpKeyFrame(frame->frameData, frame->size);
                    dumped = true;
                }
                LOGI("VIDEO_CB KEY codec=%d keyFlag=%d size=%u pts=%lld", codecID,
                     frame ? frame->flags : 0,
                     frame ? frame->size : 0,
                     static_cast<unsigned long long>(frame ? frame->presentationTs : 0));
            }
            FLOGI("VIDEO_CB codec=%d keyFlag=%d size=%u pts=%lld idx=%u", codecID,
                 frame ? frame->flags : 0,
                 frame ? frame->size : 0,
                 static_cast<unsigned long long>(frame ? frame->presentationTs : 0), frame->index);
            if (frame && frame->frameData && frame->size > 0) {
                int dumpLen = frame->size < 32 ? frame->size : 32;
                char hex[128] = {0};
                for (int i = 0; i < dumpLen; i++) {
                    sprintf(hex + i * 3, "%02X ", ((uint8_t*)frame->frameData)[i]);
                }
                FLOGI("VIDEO_CB first %d bytes: %s", dumpLen, hex);
            }
            // Lazy-create video decoder on first frame using actual remote codec
            if (!session->videoDecoder && !session->videoDecoderInitAttempted && session->decoderSurface) {
                session->videoDecoderInitAttempted = true;
                session->videoDecoder = std::make_shared<VideoDecoderPipeline>();
                session->videoDecoder->currentCodecType = (codecID == EasyRTC_CODEC_H265) ? "video/hevc" : "video/avc";
                session->videoDecoder->surface = session->decoderSurface;
                assert(session->videoDecoder);
                session->videoDecoder->onVideoSize = onRemoteVideoSizeCallback;
                session->videoDecoder->onVideoSizeUserPtr = session;
                videoDecoderStart(session->videoDecoder);
            }

            if (session->videoDecoder && frame && frame->frameData && frame->size > 0) {
                
                frameDumpWrite(&session->frameDump, FrameDumpWriter::KIND_VIDEO, frame->frameData,
                               frame->size, frame->flags);
                accumulateMediaInputBytes(session, frame->size, 0);
                videoDecoderEnqueueFrame(session->videoDecoder, frame);
            } else {
                LOGW("VIDEO_CB empty dec=%p frame=%p data=%p size=%u codec=%d",
                     session->videoDecoder ? session->videoDecoder.get() : nullptr,
                     frame,
                     frame ? frame->frameData : nullptr,
                     frame ? frame->size : 0,
                     codecID);
            }
            break;
        case EASYRTC_TRANSCEIVER_CALLBACK_AUDIO_FRAME:
            assert(frame && "invalid frame in audio callback");
            assert(frame->frameData && "invalid frame data in audio callback");
            assert(frame->size >= 0 && "invalid frame size in audio callback");
            FLOGI("AUDIO_CB codec=%d size=%u pts=%lld", codecID,
                 frame ? frame->size : 0,
                 static_cast<unsigned long long>(frame ? frame->presentationTs : 0));
            if (session->last_audio_frame_idx != 0) {
                if (session->last_audio_frame_idx + 1 != frame->index) {
                    session->audio_frame_loss_count += frame->index - session->last_audio_frame_idx - 1;
                    LOGW("AUDIO_CB frame index jump: last=%u current=%u, lost=%u", session->last_audio_frame_idx, frame->index, session->audio_frame_loss_count);
                }
            }
            // Lazy-create audio playback on first frame using actual remote codec
            if (!session->audioPlayback && !session->audioPlaybackInitAttempted) {
                session->audioPlaybackInitAttempted = true;
                session->audioPlayback = audioPlaybackCreate(static_cast<int>(codecID));
                LOGI("[CRITICAL] Recv: audio playback lazily created codec=%d", codecID);
            }

            if (session->audioPlayback && frame && frame->frameData && frame->size > 0) {
                frameDumpWrite(&session->frameDump, FrameDumpWriter::KIND_AUDIO, frame->frameData,
                               frame->size, frame->flags);
                accumulateMediaInputBytes(session, 0, frame->size);
                audioPlaybackEnqueueFrame(session->audioPlayback, frame);
                if (session->videoDecoder) {
                    session->videoDecoder->audio_master_clock_us_from_begining_to_now = estimateAudioMasterClockUs(session->audioPlayback);
                }
            }
            break;
        case EASYRTC_TRANSCEIVER_CALLBACK_KEY_FRAME_REQ:
        {
            LOGD("mediaTransceiverCallback KEY_FRAME_REQ bw=%.2f", bandwidthEstimation);
            auto p = session->videoEncoder;
            if (p) {
                p->requestKeyFramePending.store(true);
            }else {
                LOGW("mediaTransceiverCallback KEY_FRAME_REQ but video encoder not running");
            }
        }

            break;
        case EASYRTC_TRANSCEIVER_CALLBACK_BANDWIDTH:
            // LOGD("mediaTransceiverCallback BANDWIDTH=%.2f", bandwidthEstimation);
            break;
        default:
            LOGD("mediaTransceiverCallback type=%d", type);
            break;
    }
    return 0;
}

static void onRemoteVideoSizeCallback(void *userPtr, int width, int height) {
    auto *session = static_cast<MediaSession *>(userPtr);
    if (!session || !session->jvm || !session->javaObj) {
        LOGW("onRemoteVideoSizeCallback: invalid session or JVM");
        return;
    }

    JNIEnv *env = nullptr;
    bool attached = false;
    int ret = session->jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    if (ret != JNI_OK) {
        if (session->jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            attached = true;
        } else {
            LOGW("onRemoteVideoSizeCallback: failed to attach thread to JVM");
            return;
        }
    }
    if (attached) {
        jclass clazz = env->GetObjectClass(session->javaObj);
        if (clazz) {
            jmethodID mid = env->GetMethodID(clazz, "onRemoteVideoSize", "(II)V");
            if (mid) {
                env->CallVoidMethod(session->javaObj, mid, width, height);
            }
        }
        session->jvm->DetachCurrentThread();
    }
}

// ─── Native callbacks for libEasyRTC ─────────────────────────────────────────

static int connectionStateChangeCallback(void *userPtr, EASYRTC_PEER_CONNECTION_STATE state) {
    auto *session = static_cast<MediaSession *>(userPtr);
    assert(session && "Invalid session in connectionStateChangeCallback");
    LOGI("[CRITICAL] connectionStateChangeCallback PC state: session=%p state=%d", session, state);
    session->connectState = state;
    bool becameConnected = (state == 3 && session->videoEncoder);
    if (becameConnected) session->videoEncoder->requestKeyFramePending.store(true);
    // Notify Java
    if (session->jvm && session->javaObj) {
        JNIEnv *env = nullptr; bool attached = false;
        if (session->jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (session->jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
        }
        if (env) {
            jclass clazz = env->GetObjectClass(session->javaObj);
            if (clazz) {
                jmethodID mid = env->GetMethodID(clazz, "onConnectionStateChangeEvent", "(I)V");
                if (mid) env->CallVoidMethod(session->javaObj, mid, static_cast<jint>(state));
            }
            if (attached) session->jvm->DetachCurrentThread();
        }
    }
    LOGI("[CRITICAL] connectionStateChangeCallback OUT");
    return 0;
}

static int sdpCallback(void *userPtr, const int isOffer, const char *sdp) {
    auto *session = static_cast<MediaSession *>(userPtr);
    if (!session || !session->jvm || !session->javaObj) return 0;
    LOGI("[CRITICAL] PC sdp: session=%p isOffer=%d len=%zu", session, isOffer, sdp ? strlen(sdp) : 0);
    JNIEnv *env = nullptr;
    bool attached = false;
    if (session->jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (session->jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
    }
    if (env) {
        jclass clazz = env->GetObjectClass(session->javaObj);
        if (clazz) {
            jmethodID mid = env->GetMethodID(clazz, "onSdpEvent", "(ILjava/lang/String;)V");
            if (mid) {
                jstring jsdp = env->NewStringUTF(sdp ? sdp : "");
                env->CallVoidMethod(session->javaObj, mid, static_cast<jint>(isOffer), jsdp);
                env->DeleteLocalRef(jsdp);
            }
        }
        if (attached) session->jvm->DetachCurrentThread();
    }
    return 0;
}

static int dataChannelCallback(void *userPtr, EASYRTC_DATACHANNEL_CALLBACK_TYPE_E type,
                                BOOL isBinary, const char *msgData, int msgLen) {
    auto *session = static_cast<MediaSession *>(userPtr);
    if (!session || !session->jvm || !session->javaObj) return 0;
    LOGI("[CRITICAL] PC dataChannel: session=%p type=%d", session, type);
    JNIEnv *env = nullptr;
    bool attached = false;
    if (session->jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (session->jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
    }
    if (env) {
        jclass clazz = env->GetObjectClass(session->javaObj);
        if (clazz) {
            jmethodID mid = env->GetMethodID(clazz, "onDataChannelEvent", "(II[B)V");
            if (mid) {
                jbyteArray jdata = env->NewByteArray(msgLen > 0 ? msgLen : 0);
                if (msgData && msgLen > 0) {
                    env->SetByteArrayRegion(jdata, 0, msgLen, reinterpret_cast<const jbyte *>(msgData));
                }
                env->CallVoidMethod(session->javaObj, mid,
                                    static_cast<jint>(type), static_cast<jint>(isBinary), jdata);
                env->DeleteLocalRef(jdata);
            }
        }
        if (attached) session->jvm->DetachCurrentThread();
    }
    return 0;
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaSession_nativeCreate(JNIEnv *env, jobject thiz) {
    LOGI("[CRITICAL] nativeCreate ENTRY: thiz=%p", thiz);
    auto *session = new MediaSession();
    env->GetJavaVM(&session->jvm);
    session->javaObj = env->NewGlobalRef(thiz);
    LOGD("MediaSession created");
    return reinterpret_cast<jlong>(session);
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStartPreview(
        JNIEnv *env, jobject thiz, jlong sessionPtr, jobject surface) {
    LOGI("[CRITICAL] nativeStartPreview ENTRY: sessionPtr=%p surface=%p",
         reinterpret_cast<void *>(sessionPtr), surface);
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(!session->previewRunning.load() && "Preview already running");
    assert(surface && "Invalid session");

    session->previewWindow = std::shared_ptr<ANativeWindow>(ANativeWindow_fromSurface(env, surface), ANativeWindow_release);
    LOGI("[CRITICAL] nativeStartPreview: previewWindow acquired, letting camera HAL choose buffer size naturally");

    ACameraManager *cameraMgr = ACameraManager_create();
    if (!cameraMgr) {
        return -1;
    }
    defer(ACameraManager_delete(cameraMgr));

    std::string cameraId = findCameraId(session->cameraFacing);
    if (cameraId.empty()) {
        LOGE("[CRITICAL] nativeStartPreview: no camera found for facing=%d", session->cameraFacing);
        return -1;
    }

    // Log all camera-supported output formats/resolutions for diagnostics.
    {
        ACameraMetadata *metadata = nullptr;
        camera_status_t charStatus = ACameraManager_getCameraCharacteristics(
                cameraMgr, cameraId.c_str(), &metadata);
        if (charStatus == ACAMERA_OK && metadata) {
            defer(ACameraMetadata_free(metadata));
            ACameraMetadata_const_entry entry{};
            if (ACameraMetadata_getConstEntry(
                    metadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry) == ACAMERA_OK) {
                for (uint32_t i = 0; i + 4 <= entry.count; i += 4) {
                    LOGI("CAMERA output format:%d, resolution:%dx%d, input:%d",
                         entry.data.i32[i], entry.data.i32[i+1],
                         entry.data.i32[i+2], entry.data.i32[i+3]);
                }
            }
        } else {
            LOGW("[CRITICAL] nativeStartPreview: getCameraCharacteristics failed: %d", charStatus);
        }
    }

    ACameraDevice_StateCallbacks cb = {};
    cb.context = session;
    cb.onDisconnected = nullptr;
    cb.onError = nullptr;
    ACameraDevice *device = nullptr;
    camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb,
                                                          &device);
    if (camStatus != ACAMERA_OK || !device) {
        LOGE("[CRITICAL] nativeStartPreview: openCamera failed: %d", camStatus);
        return -1;
    }

    session->cameraDevice = device;
    if (!createCaptureSession(session, false)) {
        closeCamera(session);
        return -1;
    }

    session->previewRunning.store(true);
    LOGD("Preview started");
    return 0;
}


JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStopPreview(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeStopPreview ENTRY: sessionPtr=%p",
         reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");

    LOGI("[CRITICAL] nativeStopPreview: before closeCamera");
    closeCamera(session);
    session->previewWindow = nullptr;
    session->previewRunning.store(false);
    LOGD("Preview stopped");
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeAddTransceivers(
        JNIEnv *env, jobject thiz, jlong sessionPtr, jint videoCodec, jint audioCodec) {
    LOGI("[CRITICAL] nativeAddTransceivers ENTRY: sessionPtr=%p videoCodec=%d audioCodec=%d",
         reinterpret_cast<void *>(sessionPtr), videoCodec, audioCodec);
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->peerConnection && "Invalid peerConnection");
    assert(videoCodec > 0 && audioCodec > 0 && "Invalid codec IDs");
    assert(!session->transceiversAdded.load() && "Transceivers already added");

    LOGI("[CRITICAL] AddTransceivers: videoCodec=%d audioCodec=%d pc=%p",
         videoCodec, audioCodec, session->peerConnection);

    session->videoCodec = videoCodec;
    session->audioCodec = audioCodec;
    session->last_audio_frame_idx = 0;
    session->last_video_frame_idx = 0;
    session->video_frame_loss_count = 0;
    session->audio_frame_loss_count = 0;

    session->videoTrackId = 0;
    session->audioTrackId = 1;
    auto _strVideoTrack = std::to_string(session->videoTrackId);
    auto _strAudioTrack = std::to_string(session->audioTrackId);
    EasyRTC_MediaStreamTrack videoTrack{};
    videoTrack.codec = static_cast<EasyRTC_CODEC>(videoCodec);
    std::strcpy(videoTrack.streamId, "0");
    std::strcpy(videoTrack.trackId, _strVideoTrack.c_str());
    videoTrack.kind = EasyRTC_MEDIA_STREAM_TRACK_KIND_VIDEO;

    EasyRTC_RtpTransceiverInit videoInit{};
    videoInit.direction = EasyRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

    int result = EasyRTC_AddTransceiver(&session->videoTransceiver,
                                        session->peerConnection, &videoTrack, &videoInit,
                                        mediaTransceiverCallback, session);
    if (result != 0 || !session->videoTransceiver) {
        LOGE("Failed to add video transceiver: %d", result);
        assert(false && "Failed to add video transceiver");
        return -1;
    }
    LOGD("Video transceiver added,codec:%d, r: %p", videoCodec, session->videoTransceiver);

    EasyRTC_MediaStreamTrack audioTrack{};
    audioTrack.codec = static_cast<EasyRTC_CODEC>(audioCodec);
    std::strcpy(audioTrack.streamId, "0");
    std::strcpy(audioTrack.trackId, _strAudioTrack.c_str());
    audioTrack.kind = EasyRTC_MEDIA_STREAM_TRACK_KIND_AUDIO;

    EasyRTC_RtpTransceiverInit audioInit{};
    audioInit.direction = EasyRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

    result = EasyRTC_AddTransceiver(&session->audioTransceiver,
                                    session->peerConnection, &audioTrack, &audioInit,
                                    mediaTransceiverCallback, session);
    if (result != 0 || !session->audioTransceiver) {
        LOGE("Failed to add audio transceiver: %d", result);
        assert(false && "Failed to add audio transceiver");
        return -2;
    }
    LOGD("Audio transceiver added. codec:%d, r: %p", audioCodec, session->audioTransceiver);

    session->transceiversAdded.store(true);

    frameDumpInit(&session->frameDump, session->videoCodec, session->audioCodec);

    if (session->videoEncoder && session->videoTransceiver) {
        session->videoEncoder->transceiver = session->videoTransceiver;
        LOGD("Video encoder transceiver wired: %p", session->videoTransceiver);
    }
    session->statThread = std::thread([session]() {
        LOGI("MediaSession stats thread started");
        auto lastReportTime = std::chrono::steady_clock::now();
        while (session->transceiversAdded.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            if (!session->transceiversAdded.load()) {
                break;
            }
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastReportTime).count();
            if (elapsed >= 1000) {
                lastReportTime = now;
                reportMediaInputKbpsStats(session);
            }
        }
        LOGI("MediaSession stats thread exiting");
    });
    session->audioPlaybackInitAttempted = false;
    session->videoDecoderInitAttempted = false;
    return 0;
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetupVideoEncoder(
        JNIEnv *env, jobject thiz, jlong sessionPtr,
        jint codec, jint width, jint height, jint bitrate, jint fps, jint iframeInterval) {
    LOGI("[CRITICAL] nativeSetupVideoEncoder ENTRY: sessionPtr=%p codec=%d size=%dx%d bitrate=%d fps=%d iframe=%d",
         reinterpret_cast<void *>(sessionPtr), codec, width, height, bitrate, fps, iframeInterval);
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");

    const char *mime = (codec == EasyRTC_CODEC_H265) ? "video/hevc" : "video/avc";
    const bool swapWH = session->encoderSwapWH;
    const int encoderWidth = swapWH ? height : width;
    const int encoderHeight = swapWH ? width : height;
    AMediaFormat *format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, encoderWidth);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, encoderHeight);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
//    public static final int BITRATE_MODE_CBR = 2;
//    public static final int BITRATE_MODE_CBR_FD = 3;
//    public static final int BITRATE_MODE_CQ = 0;
//    public static final int BITRATE_MODE_VBR = 1;
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BITRATE_MODE, 2);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, iframeInterval);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatSurface);

    auto pipeline = std::make_shared<MediaPipeline>();
    pipeline->transceiver = session->videoTransceiver;
    pipeline->width = encoderWidth;
    pipeline->height = encoderHeight;
    pipeline->bitrate = bitrate;
    pipeline->fps = fps;
    pipeline->iframeInterval = iframeInterval;
    pipeline->format = std::shared_ptr<AMediaFormat>(format, AMediaFormat_delete);
    pipeline->mime = mime;
    session->videoEncoder = pipeline;
    LOGI("[CRITICAL] EncoderRotate setup: req=%dx%d out=%dx%d swap=%d rot=%d mime=%s",
         width, height, encoderWidth, encoderHeight, swapWH ? 1 : 0, session->encoderRotation, mime);
    return 0;
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetEncoderRotation(
        JNIEnv *env, jobject thiz, jlong sessionPtr, jint rotation) {
    LOGI("[CRITICAL] nativeSetEncoderRotation ENTRY: sessionPtr=%p rotation=%d",
         reinterpret_cast<void *>(sessionPtr), rotation);
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    int r = rotation % 360;
    if (r < 0) r += 360;
    session->encoderRotation = r;
    session->encoderSwapWH = (r == 90 || r == 270);
    LOGI("[CRITICAL] EncoderRotate config: rotation=%d swapWH=%d", session->encoderRotation,
         session->encoderSwapWH ? 1 : 0);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetDeviceId(
        JNIEnv *env, jobject thiz, jlong sessionPtr, jstring deviceId) {
    LOGI("[CRITICAL] nativeSetDeviceId ENTRY: sessionPtr=%p deviceId=%p",
         reinterpret_cast<void *>(sessionPtr), deviceId);
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");

    const char *chars = env->GetStringUTFChars(deviceId, nullptr);
    if (chars) {
        session->deviceId = chars;
        // Propagate to encoder GL bridge if already created
        if (session->encoderGlBridge) {
            session->encoderGlBridge->deviceId = chars;
        }
        env->ReleaseStringUTFChars(deviceId, chars);
        LOGI("[CRITICAL] EncoderRotate deviceId set: %s", session->deviceId.c_str());
    }
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetDecoderSurface(
        JNIEnv *env, jobject thiz, jlong sessionPtr, jobject surface) {
    LOGI("[CRITICAL] nativeSetDecoderSurface ENTRY: sessionPtr=%p surface=%p",
         reinterpret_cast<void *>(sessionPtr), surface);
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");

    if (session->decoderSurface) {
        ANativeWindow_release(session->decoderSurface);
        session->decoderSurface = nullptr;
        LOGD("Previous decoder surface released");
    }

    if (surface) {
        session->decoderSurface = ANativeWindow_fromSurface(env, surface);
        LOGD("New decoder surface set: %p", session->decoderSurface);
    }
}

static bool ensureCameraInputSurfaceTexture(JNIEnv *env, MediaSession *session, int width, int height) {
    if (!session || !session->javaObj || !session->encoderGlBridge) {
        return false;
    }
    if (session->cameraInputWindow && session->cameraInputSurfaceTexture && session->cameraInputSurfaceTextureObj) {
        return true;
    }
    jclass clazz = env->GetObjectClass(session->javaObj);
    if (!clazz) return false;
    jmethodID mid = env->GetMethodID(clazz, "createCameraInputSurfaceTexture", "(III)Landroid/graphics/SurfaceTexture;");
    if (!mid) return false;
    jobject stLocal = env->CallObjectMethod(session->javaObj, mid,
                                            static_cast<jint>(session->encoderGlBridge->cameraOesTex),
                                            static_cast<jint>(width), static_cast<jint>(height));
    if (!stLocal) {
        return false;
    }

    session->cameraInputSurfaceTextureObj = env->NewGlobalRef(stLocal);
    env->DeleteLocalRef(stLocal);
    session->cameraInputSurfaceTexture = ASurfaceTexture_fromSurfaceTexture(env, session->cameraInputSurfaceTextureObj);
    if (!session->cameraInputSurfaceTexture) {
        return false;
    }
    session->cameraInputWindow = std::shared_ptr<ANativeWindow>(ASurfaceTexture_acquireANativeWindow(session->cameraInputSurfaceTexture), ANativeWindow_release);
    if (!session->cameraInputWindow) {
        return false;
    }
    LOGI("[CRITICAL] EncoderRotate camera input ST created: st=%p window=%p tex=%u bufferSize=%dx%d",
         session->cameraInputSurfaceTexture, session->cameraInputWindow.get(),
         session->encoderGlBridge->cameraOesTex, width, height);
    return true;
}

static void startRenderThread(MediaSession *session) {
    LOGI("[CRITICAL] startRenderThread ENTRY: session=%p running=%d", session,
         session ? (session->renderThreadRunning.load() ? 1 : 0) : -1);
    if (!session || session->renderThreadRunning.load()) {
        return;
    }
    auto encoder = session->videoEncoder;
    assert(encoder && "Video encoder must be initialized before starting render thread");
    const uint32_t frameIntervalUs = static_cast<uint32_t>(1000000 / (encoder->fps > 0 ? encoder->fps : 29.97));
    session->renderThreadRunning.store(true);
    session->renderThread = std::thread([session, frameIntervalUs]() {
        LOGI("[CRITICAL] EncoderRotate render thread started");
        if (!encoderGlMakeCurrent(session->encoderGlBridge)) {
            LOGE("[CRITICAL] EncoderRotate render thread makeCurrent failed");
        }
        if (session->cameraInputSurfaceTexture && session->encoderGlBridge) {
            ASurfaceTexture_detachFromGLContext(session->cameraInputSurfaceTexture);
            int attach = ASurfaceTexture_attachToGLContext(session->cameraInputSurfaceTexture,
                                                            session->encoderGlBridge->cameraOesTex);
            LOGI("[CRITICAL] EncoderRotate render thread attachToGLContext=%d tex=%u",
                 attach, session->encoderGlBridge->cameraOesTex);
        }
        while (session->renderThreadRunning.load()) {
            if (session->cameraInputSurfaceTexture && session->encoderGlBridge && session->encoderGlBridge->initialized) {
                float m[16];
                int64_t ts = 0;
                if (encoderGlUpdateTexImage(session->cameraInputSurfaceTexture, m, &ts)) {
                    // Keep SurfaceTexture drained, but only feed encoder when connected.
                    if (session->connectState == 3) {
                        encoderGlSetInputTransform(session->encoderGlBridge, m);
                        if (!encoderGlRenderFrame(session->encoderGlBridge, static_cast<long long>(ts))) {
                            LOGW("[CRITICAL] EncoderRotate render thread frame render failed");
                            assert(false && "Encoder render failed");
                        }
                    } else {
                        FLOGI("[CRITICAL] EncoderRotate render thread: not connected, skipping frame");
                    }
                }else {
                    LOGE("[CRITICAL] EncoderRotate render thread updateTexImage failed");
                    assert(false && "Encoder updateTexImage failed");
                }
            }
            usleep(frameIntervalUs);
        }
        if (session->cameraInputSurfaceTexture) {
            LOGI("[CRITICAL] EncoderRotate render thread: detaching SurfaceTexture");
            ASurfaceTexture_detachFromGLContext(session->cameraInputSurfaceTexture);
        }
        LOGI("[CRITICAL] EncoderRotate render thread stopped");
    });
    pthread_t th = session->renderThread.native_handle();
    sched_param sch_params;
    sch_params.sched_priority = 0;//sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(th, SCHED_FIFO, &sch_params);
}

static void stopRenderThread(MediaSession *session) {
    LOGI("[CRITICAL] stopRenderThread ENTRY: session=%p running=%d", session,
         session ? (session->renderThreadRunning.load() ? 1 : 0) : -1);
    if (!session) return;
    session->renderThreadRunning.store(false);
    if (session->renderThread.joinable()) {
        session->renderThread.join();
    }
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStartSend(JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeStartSend ENTRY: sessionPtr=%p", reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->videoEncoder);
    assert(!session->videoEncoder->running.load() && "videoEncoder already running");
    LOGI("[CRITICAL] StartSend: initializing video encoder");
    auto p = session->videoEncoder;
    if (!p->initEncoder()) {
        LOGE("[CRITICAL] StartSend: initEncoder FAILED");
        return -1;
    }
    assert(p->encoder);
    session->encoderGlBridge = encoderGlCreate(p->encoderInputSurface.get(), p->width, p->height, session->encoderRotation);
    if (!session->deviceId.empty() && session->encoderGlBridge) {
        session->encoderGlBridge->deviceId = session->deviceId;
    }
    if (session->encoderGlBridge && session->encoderGlBridge->initialized) {
        LOGI("[CRITICAL] EncoderRotate pipeline: camera->offscreen->encoder active rot=%d size=%dx%d",
             session->encoderRotation, p->width, p->height);
    } else {
        LOGW("[CRITICAL] EncoderRotate pipeline: GL bridge not fully initialized");
    }
    LOGI("[CRITICAL] EncoderRotate start send: rotation=%d swapWH=%d",
         session->encoderRotation, session->encoderSwapWH ? 1 : 0);
    if (!ensureCameraInputSurfaceTexture(env, session,
            session->encoderSwapWH ? p->height : p->width,
            session->encoderSwapWH ? p->width : p->height)) {
        LOGW("[CRITICAL] EncoderRotate failed to create camera input surface texture");
    }
    LOGI("[CRITICAL] nativeStartSend: before startRenderThread");
    startRenderThread(session);
    media_status_t status = AMediaCodec_start(p->encoder);
    if (status != AMEDIA_OK) {
        LOGE("Failed to start encoder: %d", status);
        return -1;
    }
    p->startEncoder(session);
    // pthread_t th = p->outputThread.native_handle();
    // sched_param sch_params;
    // sch_params.sched_priority = 0;//sched_get_priority_max(SCHED_FIFO);
    // pthread_setschedparam(th, SCHED_FIFO, &sch_params);
    if (session->cameraDevice) {
        std::lock_guard<std::mutex> lock(session->cameraMutex);
        releaseCaptureSession(session);
        if (!createCaptureSession(session, true)) {
            LOGE("Failed to recreate capture session with encoder");
        }
    }
    LOGD("Video encoder started");

    if (session->audioTransceiver) {
        assert(!session->audioCapture && "audioCapture already exists");
        session->audioCapture = std::make_shared<AudioCapturePipeline>();
        session->audioCapture->audioTransceiver = session->audioTransceiver;
        LOGI("[CRITICAL] StartSend: starting audio capture");
        audioCaptureStart(session);
    }
    LOGI("[CRITICAL] StartSend: DONE encoder=%p audioCapture=%p",
         p->encoder, session->audioCapture.get());
    return 0;
}


JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSwitchCamera(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeSwitchCamera ENTRY: sessionPtr=%p",
         reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");

    std::lock_guard<std::mutex> lock(session->cameraMutex);
    if (session->captureSession) {
        ACameraCaptureSession_stopRepeating(session->captureSession);
        session->captureSession = nullptr;
    }
    if (session->cameraDevice) {
        ACameraDevice_close(session->cameraDevice);
        session->cameraDevice = nullptr;
    }
    releaseCaptureSessionOutputs(session);

    session->cameraFacing = (session->cameraFacing == 0) ? 1 : 0;

    ACameraManager *cameraMgr = ACameraManager_create();
    if (!cameraMgr) { return; }

    std::string cameraId = findCameraId(session->cameraFacing);
    if (!cameraId.empty()) {
        ACameraDevice_StateCallbacks cb = {};
        cb.context = session;
        cb.onDisconnected = nullptr;
        cb.onError = nullptr;
        ACameraDevice *device = nullptr;
        camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb,
                                                              &device);
        if (camStatus == ACAMERA_OK && device) {
            session->cameraDevice = device;
            createCaptureSession(session,
                                 session->videoEncoder && session->videoEncoder->running.load());
        }
    }
    ACameraManager_delete(cameraMgr);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeRequestKeyFrame(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeRequestKeyFrame ENTRY: sessionPtr=%p",
         reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "session is null");
    auto p = session->videoEncoder;
    if (!p) {
        LOGW("[CRITICAL] nativeRequestKeyFrame: video encoder not running, cannot request key frame");
        return;
    }
    p->requestKeyFramePending.store(true);
}


JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeRelease(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeRelease ENTRY: sessionPtr=%p",
         reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    if (!session) return;
    assert(!session->videoTransceiver && "videoTransceiver should have been removed before release");
    assert(!session->audioTransceiver && "audioTransceiver should have been removed before release");
    LOGI("[CRITICAL] Release: cleaning up session");

    closeCamera(session);
    session->previewWindow = nullptr;
    session->previewRunning.store(false);

    if (session->decoderSurface) {
        ANativeWindow_release(session->decoderSurface);
        session->decoderSurface = nullptr;
    }

    session->cameraInputWindow = nullptr;
    if (session->cameraInputSurfaceTexture) {
        ASurfaceTexture_release(session->cameraInputSurfaceTexture);
        session->cameraInputSurfaceTexture = nullptr;
    }
    if (session->cameraInputSurfaceTextureObj) {
        env->DeleteGlobalRef(session->cameraInputSurfaceTextureObj);
        session->cameraInputSurfaceTextureObj = nullptr;
    }

    if (session->javaObj) {
        env->DeleteGlobalRef(session->javaObj);
        session->javaObj = nullptr;
    }

    frameDumpClose(&session->frameDump);

    delete session;
    LOGD("MediaSession released");
}

// ─── PeerConnection lifecycle ────────────────────────────────────────────────

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaSession_nativeCreatePeerConnection(
        JNIEnv *env, jobject thiz, jlong sessionPtr,
        jstring stunUrl, jstring turnUrl, jstring turnUser, jstring turnPass) {
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(!session->peerConnection && "PeerConnection already exists");

    EasyRTC_Configuration config{};
    config.iceTransportPolicy = EasyRTC_ICE_TRANSPORT_POLICY_ALL;

    const char *stun = env->GetStringUTFChars(stunUrl, nullptr);
    const char *turn = env->GetStringUTFChars(turnUrl, nullptr);
    const char *user = env->GetStringUTFChars(turnUser, nullptr);
    const char *pass = env->GetStringUTFChars(turnPass, nullptr);
    assert(stun && "STUN URL is null");
    assert(turn && "TURN URL is null");
    assert(user && "TURN username is null");
    assert(pass && "TURN password is null");

    if (strlen(stun) > 0) {
        strncpy(config.iceServers[0].urls, stun, MAX_ICE_CONFIG_URI_LEN);
    }
    if (strlen(turn) > 0) {
        strncpy(config.iceServers[1].urls, turn, MAX_ICE_CONFIG_URI_LEN);
        if (user) strncpy(config.iceServers[1].username, user, MAX_ICE_CONFIG_USER_NAME_LEN);
        if (pass) strncpy(config.iceServers[1].credential, pass, MAX_ICE_CONFIG_CREDENTIAL_LEN);
    }
    LOGI("[CRITICAL] CreatePeerConnection: stun=%s turn=%s user=%s pass=%s", stun, turn, user, pass);

    env->ReleaseStringUTFChars(stunUrl, stun);
    env->ReleaseStringUTFChars(turnUrl, turn);
    env->ReleaseStringUTFChars(turnUser, user);
    env->ReleaseStringUTFChars(turnPass, pass);

    EasyRTC_PeerConnection pc = nullptr;
    int result = EasyRTC_CreatePeerConnection(&pc, &config, connectionStateChangeCallback, session);
    if (result != 0 || !pc) {
        LOGE("[CRITICAL] EasyRTC_CreatePeerConnection FAILED: %d", result);
        assert(false && "Failed to create PeerConnection");
        return 0;
    }
    session->peerConnection = pc;
    session->connectState = 1; // NEW
    LOGI("[CRITICAL] PC created: session=%p pc=%p", session, pc);
    return reinterpret_cast<jlong>(pc);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeReleasePeerConnection(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    session->transceiversAdded.store(false);

    // 1. Stop send pipeline (render thread, encoder, audio capture)
    LOGI("[CRITICAL] StopSend: stopping audio capture and video encoder");
    audioCaptureStop(session);
    LOGI("[CRITICAL] nativeStopSend: before stopRenderThread");
    stopRenderThread(session);
    LOGI("[CRITICAL] nativeStopSend: before encoderGlRelease");
    encoderGlRelease(session->encoderGlBridge);

    if (session->videoEncoder) {
        auto p = session->videoEncoder;
        p->running.exchange(false);
        LOGI("[CRITICAL] StopSend: signaling EOS and stopping encoder");
        if (p->encoder) AMediaCodec_signalEndOfInputStream(p->encoder);
        LOGI("[CRITICAL] StopSend: join output thread");
        if (p->outputThread.joinable()) p->outputThread.join();
        LOGI("[CRITICAL] StopSend: stopping encoder");
        if (p->encoder) AMediaCodec_stop(p->encoder);
        LOGI("[CRITICAL] StopSend: deleting encoder");
        if (p->encoder) AMediaCodec_delete(p->encoder);
        p->encoder = nullptr;
    }

    // 4. Free children BEFORE releasing parent PC
    if (session->dataChannel) {
        EasyRTC_FreeDataChannel(&session->dataChannel);
        session->dataChannel = nullptr;
    }
    if (session->videoTransceiver) {
        EasyRTC_FreeTransceiver(&session->videoTransceiver);
        session->videoTransceiver = nullptr;
    }
    if (session->audioTransceiver) {
        EasyRTC_FreeTransceiver(&session->audioTransceiver);
        session->audioTransceiver = nullptr;
    }

    if (session->cameraDevice && session->cameraInputWindow) {
        std::lock_guard<std::mutex> lock(session->cameraMutex);

        if (session->cameraInputSurfaceTexture) {
            ASurfaceTexture_release(session->cameraInputSurfaceTexture);
            session->cameraInputSurfaceTexture = nullptr;
        }
        if (session->cameraInputSurfaceTextureObj) {
            env->DeleteGlobalRef(session->cameraInputSurfaceTextureObj);
            session->cameraInputSurfaceTextureObj = nullptr;
        }
        session->cameraInputWindow.reset();

        releaseCaptureSession(session);
        if (session->previewWindow) createCaptureSession(session, false);
    }
    LOGI("[CRITICAL] StopSend: DONE");

    // 2. Stop recv pipeline (audio playback, video decoder)
    LOGI("[CRITICAL] StopRecv: releasing audio playback and video decoder");
    if (session->audioPlayback) {
        audioPlaybackRelease(session->audioPlayback);
        session->audioPlayback = nullptr;
    }
    if (session->videoDecoder) {
        videoDecoderRelease(session->videoDecoder);
        session->videoDecoder = nullptr;
    }
    LOGI("[CRITICAL] StopRecv: DONE");

    // 3. Join stats thread
    if (session->statThread.joinable()) {
        LOGI("[CRITICAL] RemoveTransceivers: joining stats thread");
        session->statThread.join();
    }

    //
    if (session->peerConnection) {
        LOGI("[CRITICAL] PC releasing: session=%p", session);
        EasyRTC_ReleasePeerConnection(&session->peerConnection);
        session->peerConnection = nullptr;
        LOGI("[CRITICAL] PC released: session=%p", session);
    }

}

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaSession_nativeAddDataChannel(
        JNIEnv *env, jobject thiz, jlong sessionPtr, jstring name) {
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->peerConnection && "Invalid peer connection");
    assert(!session->dataChannel && "Data channel already exists");
    const char *cname = env->GetStringUTFChars(name, nullptr);
    EasyRTC_AddDataChannel(&session->dataChannel, session->peerConnection, cname, dataChannelCallback, session);
    env->ReleaseStringUTFChars(name, cname);
    LOGI("[CRITICAL] Data channel added: session=%p dc=%p", session, session->dataChannel);
    return reinterpret_cast<jlong>(session->dataChannel);
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeDataChannelSend(
        JNIEnv *env, jobject thiz, jlong sessionPtr, jboolean isBinary, jbyteArray data) {
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    if(!session->dataChannel)
    {
        LOGI("[CRITICAL] Data channel not added when sending data...");
        return -1;
    }
    jsize len = env->GetArrayLength(data);
    jbyte *bytes = env->GetByteArrayElements(data, nullptr);
    int result = EasyRTC_DataChannelSend(session->dataChannel, isBinary ? TRUE : FALSE,
                                         reinterpret_cast<const char *>(bytes), len);
    env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
    return result;
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeCreateOffer(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->peerConnection && "Invalid peer connection");
    EasyRTC_CreateOffer(session->peerConnection, sdpCallback, session);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeCreateAnswer(
        JNIEnv *env, jobject thiz, jlong sessionPtr, jstring offerSdp) {
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->peerConnection && "Invalid peer connection");
    const char *osdp = env->GetStringUTFChars(offerSdp, nullptr);
    EasyRTC_CreateAnswer(session->peerConnection, osdp, sdpCallback, session);
    env->ReleaseStringUTFChars(offerSdp, osdp);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetRemoteDescription(
        JNIEnv *env, jobject thiz, jlong sessionPtr, jstring sdp) {
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->peerConnection && "Invalid peer connection");
    const char *sdpc = env->GetStringUTFChars(sdp, nullptr);
    EasyRTC_SetRemoteDescription(session->peerConnection, sdpc);
    env->ReleaseStringUTFChars(sdp, sdpc);
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeGetIceCandidateType(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->peerConnection && "Invalid peer connection");
    EasyRTC_IceCandidatePairStats stats{};
    int ret = EasyRTC_GetIceCandidatePairStats(session->peerConnection, &stats);
    if (ret != 0) return -1;
    return static_cast<jint>(stats.local_iceCandidateType);
}

}
