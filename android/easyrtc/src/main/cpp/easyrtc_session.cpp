#include "easyrtc_session.h"
#include "easyrtc_common.h"
#include "easyrtc_media.h"
#include "easyrtc_audio.h"
#include "easyrtc_audio_playback.h"
#include "easyrtc_video_decoder.h"
#include "easyrtc_frame_dump.h"
#include "easyrtc_encoder_gl.h"
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
    std::atomic<uint64_t> gVideoCbCount{0};
    std::atomic<uint64_t> gVideoCbBytes{0};
    std::atomic<uint64_t> gVideoCbAnnexB{0};
    std::atomic<uint64_t> gVideoCbAvcc{0};

}

static void notifyInputKbpsStats(MediaSession *session,
                                 double videoKbps,
                                 double audioKbps,
                                 double totalKbps) {
    if (!session || !session->jvm || !session->javaObj) {
        return;
    }

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

static void updateMediaInputKbpsStats(MediaSession *session, uint32_t videoBytes, uint32_t audioBytes) {
    if (!session) {
        return;
    }
    if (videoBytes > 0) {
        session->mediaInputVideoBytesWindow.fetch_add(videoBytes, std::memory_order_relaxed);
    }
    if (audioBytes > 0) {
        session->mediaInputAudioBytesWindow.fetch_add(audioBytes, std::memory_order_relaxed);
    }

    const int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    int64_t lastNs = session->mediaInputLastReportNs.load(std::memory_order_relaxed);
    if (lastNs == 0) {
        session->mediaInputLastReportNs.compare_exchange_strong(lastNs, nowNs, std::memory_order_relaxed);
        return;
    }
    const int64_t durationNs = nowNs - lastNs;
    constexpr int64_t kReportIntervalNs = 1000LL * 1000LL * 1000LL;
    if (durationNs < kReportIntervalNs) {
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

static void releaseCaptureSession(MediaSession *s) {
    LOGI("[CRITICAL] releaseCaptureSession ENTRY: s=%p", s);
    if (s->captureSession) {
        ACameraCaptureSession_stopRepeating(s->captureSession);
        ACameraCaptureSession_close(s->captureSession);
        s->captureSession = nullptr;
    }
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
                reinterpret_cast<ACameraWindowType *>(s->cameraInputWindow), &s->cameraInputOutput);
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
                reinterpret_cast<ACameraWindowType *>(s->previewWindow), &s->previewOutput);
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
                reinterpret_cast<ACameraWindowType *>(s->cameraInputWindow), &s->cameraInputTarget);
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
                reinterpret_cast<ACameraWindowType *>(s->previewWindow), &s->previewTarget);
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

static void closeCamera(MediaSession *s) {
    LOGI("[CRITICAL] closeCamera ENTRY: s=%p", s);
    std::lock_guard<std::mutex> lock(s->cameraMutex);
    releaseCaptureSession(s);
    if (s->cameraDevice) {
        ACameraDevice_close(s->cameraDevice);
        s->cameraDevice = nullptr;
    }
}

static int mediaTransceiverCallback(void *userPtr,
                                    EASYRTC_TRANSCEIVER_CALLBACK_TYPE_E type,
                                    EasyRTC_CODEC codecID,
                                    EasyRTC_Frame *frame,
                                    double bandwidthEstimation) {

    auto *session = static_cast<MediaSession *>(userPtr);
    assert(session && "Invalid session in transceiver callback");

    switch (type) {
        case EASYRTC_TRANSCEIVER_CALLBACK_VIDEO_FRAME:
#if 0
            static auto f = frame->presentationTs;
            LOGD("VIDEO PTS:%llu, DELTA:%llu， size:%d， flag:%d, duration:%llu", frame->presentationTs, frame->presentationTs - f, frame->size, frame->flags, frame->duration);
            f = frame->presentationTs;
#endif
            if (session->videoDecoder && frame && frame->frameData && frame->size > 0) {
                const uint8_t *p = frame->frameData;
                const uint32_t n = frame->size;
                bool annexB = false;
                bool avcc = false;
                int nalType = -1;
                if (n >= 4) {
                    if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01) {
                        annexB = true;
                        if (n >= 5) nalType = p[4] & 0x1F;
                    } else if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01) {
                        annexB = true;
                        if (n >= 4) nalType = p[3] & 0x1F;
                    } else {
                        avcc = true;
                        if (n >= 5) nalType = p[4] & 0x1F;
                    }
                }

                uint64_t idx = gVideoCbCount.fetch_add(1) + 1;
                gVideoCbBytes.fetch_add(n);
                if (annexB) gVideoCbAnnexB.fetch_add(1);
                if (avcc) gVideoCbAvcc.fetch_add(1);

                if (idx <= 8 || (idx % 120) == 0) {
                    const uint64_t totalBytes = gVideoCbBytes.load();
                    const uint64_t annexBCount = gVideoCbAnnexB.load();
                    const uint64_t avccCount = gVideoCbAvcc.load();
                    LOGD("VIDEO_CB idx=%lld codec=%d size=%u nal=%d fmt=%s pts=%lld dec=%p b0=%02X b1=%02X b2=%02X b3=%02X avg=%llu annexb=%llu avcc=%llu",
                         static_cast<unsigned long long>(idx),
                         codecID,
                         n,
                         nalType,
                         annexB ? "annexb" : (avcc ? "avcc" : "unknown"),
                         static_cast<unsigned long long>(frame->presentationTs),
                         session->videoDecoder ? session->videoDecoder.get() : nullptr,
                         n > 0 ? p[0] : 0,
                         n > 1 ? p[1] : 0,
                         n > 2 ? p[2] : 0,
                         n > 3 ? p[3] : 0,
                         static_cast<unsigned long long>(idx ? totalBytes / idx : 0),
                         static_cast<unsigned long long>(annexBCount),
                         static_cast<unsigned long long>(avccCount));
                }
                frameDumpWrite(&session->frameDump, FrameDumpWriter::KIND_VIDEO, frame->frameData,
                               frame->size, frame->flags);
                updateMediaInputKbpsStats(session, n, 0);
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
            FLOGI("AUDIO_CB codec=%d size=%u pts=%lld", codecID,
                 frame ? frame->size : 0,
                 static_cast<unsigned long long>(frame ? frame->presentationTs : 0));
//            LOGD("mediaTransceiverCallback AUDIO codec=%d size=%u pts=%llu", codecID,
//                    frame ? frame->size : 0,
//                    static_cast<unsigned long long>(frame ? frame->presentationTs : 0));
            if (session->audioPlayback && frame && frame->frameData && frame->size > 0) {
                frameDumpWrite(&session->frameDump, FrameDumpWriter::KIND_AUDIO, frame->frameData,
                               frame->size, frame->flags);
                updateMediaInputKbpsStats(session, 0, frame->size);
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

    session->previewWindow = ANativeWindow_fromSurface(env, surface);

    ACameraManager *cameraMgr = ACameraManager_create();
    if (!cameraMgr) {
        ANativeWindow_release(session->previewWindow);
        session->previewWindow = nullptr;
        return -1;
    }

    std::string cameraId = findCameraId(session->cameraFacing);
    if (cameraId.empty()) {
        ANativeWindow_release(session->previewWindow);
        session->previewWindow = nullptr;
        ACameraManager_delete(cameraMgr);
        return -1;
    }

    ACameraDevice_StateCallbacks cb = {};
    cb.context = session;
    cb.onDisconnected = nullptr;
    cb.onError = nullptr;
    ACameraDevice *device = nullptr;
    camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb,
                                                          &device);
    ACameraManager_delete(cameraMgr);

    if (camStatus != ACAMERA_OK || !device) {
        ANativeWindow_release(session->previewWindow);
        session->previewWindow = nullptr;
        return -1;
    }

    session->cameraDevice = device;
    if (!createCaptureSession(session, false)) {
        closeCamera(session);
        ANativeWindow_release(session->previewWindow);
        session->previewWindow = nullptr;
        return -1;
    }

    session->previewRunning.store(true);
    LOGD("Preview started");
    return 0;
}


JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetState(
        JNIEnv *env, jobject thiz, jlong sessionPtr,
        jint state) {
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    LOGI("[CRITICAL] nativeSetState ENTRY: session=%p %d -> %d", session, session->connectState,
         state);
    const bool becameConnected = (session->connectState != 3 && state == 3);
    session->connectState = state;
    auto p = session->videoEncoder;
    if (becameConnected && p) {
        p->requestKeyFramePending.store(true);
        LOGI("[CRITICAL] nativeSetState: queued key frame request after connected");
    }
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
    if (session->previewWindow) {
        ANativeWindow_release(session->previewWindow);
        session->previewWindow = nullptr;
    }
    session->previewRunning.store(false);
    LOGD("Preview stopped");
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetPeerConnection(
        JNIEnv *env, jobject thiz, jlong sessionPtr, jlong peerConnectionHandle) {
    LOGI("[CRITICAL] nativeSetPeerConnection ENTRY: sessionPtr=%p peerConnectionHandle=%p",
         reinterpret_cast<void *>(sessionPtr), reinterpret_cast<void *>(peerConnectionHandle));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");


    session->peerConnection = reinterpret_cast<EasyRTC_PeerConnection>(peerConnectionHandle);
    session->transceiversAdded.store(false);
    LOGD("MediaSession peerConnection updated: %p", session->peerConnection);
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

    EasyRTC_MediaStreamTrack videoTrack{};
    videoTrack.codec = static_cast<EasyRTC_CODEC>(videoCodec);
    std::strcpy(videoTrack.streamId, "0");
    std::strcpy(videoTrack.trackId, "0");
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
    std::strcpy(audioTrack.trackId, "1");
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
    LOGI("[CRITICAL] AddTransceivers: SUCCESS video=%p audio=%p",
         session->videoTransceiver, session->audioTransceiver);

    frameDumpInit(&session->frameDump, session->videoCodec, session->audioCodec);

    if (session->videoEncoder && session->videoTransceiver) {
        session->videoEncoder->transceiver = session->videoTransceiver;
        LOGD("Video encoder transceiver wired: %p", session->videoTransceiver);
    }

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

    const char *mime = (codec == 6) ? "video/hevc" : "video/avc";
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
    session->cameraInputWindow = ASurfaceTexture_acquireANativeWindow(session->cameraInputSurfaceTexture);
    if (!session->cameraInputWindow) {
        return false;
    }
    LOGI("[CRITICAL] EncoderRotate camera input ST created: st=%p window=%p tex=%u",
         session->cameraInputSurfaceTexture, session->cameraInputWindow,
         session->encoderGlBridge->cameraOesTex);
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
    if (!ensureCameraInputSurfaceTexture(env, session, p->width, p->height)) {
        LOGW("[CRITICAL] EncoderRotate failed to create camera input surface texture");
    }
    LOGI("[CRITICAL] nativeStartSend: before startRenderThread");
    startRenderThread(session);
    media_status_t status = AMediaCodec_start(p->encoder);
    if (status != AMEDIA_OK) {
        LOGE("Failed to start encoder: %d", status);
        return -1;
    }

    p->running.store(true);
    p->outputThread = std::thread([](void *sessionPtr) {
        outputThreadFunc(sessionPtr);
    }, session);
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
Java_cn_easyrtc_media_MediaSession_nativeStartRecv(JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeStartRecv ENTRY: sessionPtr=%p", reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");

    LOGI("[CRITICAL] StartRecv: creating audio playback and video decoder codec=%d", session->videoCodec);

    session->audioPlayback = audioPlaybackCreate(session->audioCodec);

    int decoderWidth = 720;
    int decoderHeight = 1280;
    if (session->videoEncoder) {
        decoderWidth = session->videoEncoder->width;
        decoderHeight = session->videoEncoder->height;
    }

    session->videoDecoder = videoDecoderCreate(session->decoderSurface,
                                               session->videoCodec,
                                               decoderWidth,
                                               decoderHeight);
    if (!session->videoDecoder) {
        LOGE("[CRITICAL] StartRecv: videoDecoderCreate FAILED");
        return;
    }
    session->videoDecoder->onVideoSize = onRemoteVideoSizeCallback;
    session->videoDecoder->onVideoSizeUserPtr = session;
    if (videoDecoderStart(session->videoDecoder) != 0) {
        LOGE("[CRITICAL] StartRecv: videoDecoderStart FAILED");
        videoDecoderRelease(session->videoDecoder);
        session->videoDecoder = nullptr;
        return;
    }
    LOGI("[CRITICAL] StartRecv: DONE playback=%p decoder=%p",
         session->audioPlayback.get(), session->videoDecoder.get());
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStopSend(JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeStopSend ENTRY: sessionPtr=%p", reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");

    LOGI("[CRITICAL] StopSend: stopping audio capture and video encoder");

    audioCaptureStop(session);

    // Must stop the render thread BEFORE releasing GL resources,
    // otherwise the render thread may call glDrawElements on deleted VBO/IBO.
    LOGI("[CRITICAL] nativeStopSend: before stopRenderThread");
    stopRenderThread(session);
    encoderGlRelease(session->encoderGlBridge);

    if (session->videoEncoder) {
    auto p = session->videoEncoder;

        if (p->running.exchange(false)) {
            LOGI("[CRITICAL] StopSend: signaling EOS and stopping encoder");
            if (p->encoder) { AMediaCodec_signalEndOfInputStream(p->encoder); }
            if (p->outputThread.joinable()) { p->outputThread.join(); }
            if (p->encoder) { AMediaCodec_stop(p->encoder); }
        }
        if (p->encoder) {
            AMediaCodec_delete(p->encoder);
            p->encoder = nullptr;
        }
    }

    if (session->cameraDevice) {
        std::lock_guard<std::mutex> lock(session->cameraMutex);
        releaseCaptureSession(session);
        if (session->previewWindow) {
            createCaptureSession(session, false);
        }
    }

    LOGI("[CRITICAL] StopSend: DONE");
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStopRecv(JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeStopRecv ENTRY: sessionPtr=%p", reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");

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
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSwitchCamera(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeSwitchCamera ENTRY: sessionPtr=%p",
         reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");

    std::lock_guard<std::mutex> lock(session->cameraMutex);
    releaseCaptureSession(session);
    if (session->cameraDevice) {
        ACameraDevice_close(session->cameraDevice);
        session->cameraDevice = nullptr;
    }

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

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaSession_nativeGetVideoTransceiver(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeGetVideoTransceiver ENTRY: sessionPtr=%p",
         reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    return session ? reinterpret_cast<jlong>(session->videoTransceiver) : 0;
}

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaSession_nativeGetAudioTransceiver(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeGetAudioTransceiver ENTRY: sessionPtr=%p",
         reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    return session ? reinterpret_cast<jlong>(session->audioTransceiver) : 0;
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeRemoveTransceivers(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeRemoveTransceivers ENTRY: sessionPtr=%p",
         reinterpret_cast<void *>(sessionPtr));

    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    if (!session) return;

    LOGI("[CRITICAL] RemoveTransceivers: video=%p audio=%p",
         session->videoTransceiver, session->audioTransceiver);

    if (session->videoTransceiver) {
        EasyRTC_FreeTransceiver(&session->videoTransceiver);
        session->videoTransceiver = nullptr;
    }
    if (session->audioTransceiver) {
        EasyRTC_FreeTransceiver(&session->audioTransceiver);
        session->audioTransceiver = nullptr;
    }
    session->transceiversAdded.store(false);
    LOGI("[CRITICAL] RemoveTransceivers: DONE");
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeRelease(
        JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeRelease ENTRY: sessionPtr=%p",
         reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    if (!session) return;

    LOGI("[CRITICAL] Release: cleaning up session");

    closeCamera(session);
    if (session->previewWindow) {
        ANativeWindow_release(session->previewWindow);
        session->previewWindow = nullptr;
    }
    session->previewRunning.store(false);

    if (session->decoderSurface) {
        ANativeWindow_release(session->decoderSurface);
        session->decoderSurface = nullptr;
    }

    if (session->cameraInputWindow) {
        ANativeWindow_release(session->cameraInputWindow);
        session->cameraInputWindow = nullptr;
    }
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

}
