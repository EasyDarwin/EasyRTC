#include "easyrtc_session.h"
#include "easyrtc_media.h"
#include "easyrtc_audio.h"
#include "easyrtc_audio_playback.h"
#include "easyrtc_video_decoder.h"
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadataTags.h>
#include <cassert>
#include <jni.h>
#include <cstring>
#include <thread>
#include <mutex>

static void releaseCaptureSession(MediaSession* s) {
    if (s->captureSession) {
        ACameraCaptureSession_stopRepeating(s->captureSession);
        ACameraCaptureSession_close(s->captureSession);
        s->captureSession = nullptr;
    }
    if (s->captureRequest) { ACaptureRequest_free(s->captureRequest); s->captureRequest = nullptr; }
    if (s->encoderTarget) { ACameraOutputTarget_free(s->encoderTarget); s->encoderTarget = nullptr; }
    if (s->previewTarget) { ACameraOutputTarget_free(s->previewTarget); s->previewTarget = nullptr; }
    if (s->encoderOutput) { ACaptureSessionOutput_free(s->encoderOutput); s->encoderOutput = nullptr; }
    if (s->previewOutput) { ACaptureSessionOutput_free(s->previewOutput); s->previewOutput = nullptr; }
    if (s->outputContainer) { ACaptureSessionOutputContainer_free(s->outputContainer); s->outputContainer = nullptr; }
}

static bool createCaptureSession(MediaSession* s, bool withEncoder) {
    camera_status_t camStatus;

    camStatus = ACaptureSessionOutputContainer_create(&s->outputContainer);
    if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }

    if (withEncoder && s->videoEncoder && s->videoEncoder->window) {
        camStatus = ACaptureSessionOutput_create(
                reinterpret_cast<ACameraWindowType*>(s->videoEncoder->window), &s->encoderOutput);
        if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }
        camStatus = ACaptureSessionOutputContainer_add(s->outputContainer, s->encoderOutput);
        if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }
    }

    if (s->previewWindow) {
        camStatus = ACaptureSessionOutput_create(
                reinterpret_cast<ACameraWindowType*>(s->previewWindow), &s->previewOutput);
        if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }
        camStatus = ACaptureSessionOutputContainer_add(s->outputContainer, s->previewOutput);
        if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }
    }

    camStatus = ACameraDevice_createCaptureRequest(s->cameraDevice,
            withEncoder ? TEMPLATE_RECORD : TEMPLATE_PREVIEW, &s->captureRequest);
    if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }

    if (withEncoder && s->videoEncoder && s->videoEncoder->window) {
        camStatus = ACameraOutputTarget_create(
                reinterpret_cast<ACameraWindowType*>(s->videoEncoder->window), &s->encoderTarget);
        if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }
        camStatus = ACaptureRequest_addTarget(s->captureRequest, s->encoderTarget);
        if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }
    }

    if (s->previewWindow) {
        camStatus = ACameraOutputTarget_create(
                reinterpret_cast<ACameraWindowType*>(s->previewWindow), &s->previewTarget);
        if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }
        camStatus = ACaptureRequest_addTarget(s->captureRequest, s->previewTarget);
        if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }
    }

    uint8_t afMode = ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    ACaptureRequest_setEntry_u8(s->captureRequest, ACAMERA_CONTROL_AF_MODE, 1, &afMode);

    static const ACameraCaptureSession_stateCallbacks sessionCallbacks = { nullptr, nullptr, nullptr, nullptr };
    camStatus = ACameraDevice_createCaptureSession(s->cameraDevice, s->outputContainer,
            &sessionCallbacks, &s->captureSession);
    if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }

    camStatus = ACameraCaptureSession_setRepeatingRequest(s->captureSession, nullptr, 1,
            &s->captureRequest, nullptr);
    if (camStatus != ACAMERA_OK) { releaseCaptureSession(s); return false; }

    return true;
}

static void closeCamera(MediaSession* s) {
    std::lock_guard<std::mutex> lock(s->cameraMutex);
    releaseCaptureSession(s);
    if (s->cameraDevice) {
        ACameraDevice_close(s->cameraDevice);
        s->cameraDevice = nullptr;
    }
}

static int mediaTransceiverCallback(void* userPtr,
        EASYRTC_TRANSCEIVER_CALLBACK_TYPE_E type,
        EasyRTC_CODEC codecID,
        EasyRTC_Frame* frame,
        double bandwidthEstimation) {

    auto* session = static_cast<MediaSession*>(userPtr);
    assert(session && "Invalid session in transceiver callback");

    switch (type) {
        case EASYRTC_TRANSCEIVER_CALLBACK_VIDEO_FRAME:
            if (session->videoDecoder && frame && frame->frameData && frame->size > 0) {
                LOGD("mediaTransceiverCallback VIDEO codec=%d size=%u pts=%llu", codecID,
                     frame ? frame->size : 0,
                     static_cast<unsigned long long>(frame ? frame->presentationTs : 0));
                videoDecoderEnqueueFrame(session->videoDecoder, frame->frameData, static_cast<int32_t>(frame->size));
            }else {
                LOGD("mediaTransceiverCallback VIDEO ");
            }
            break;
        case EASYRTC_TRANSCEIVER_CALLBACK_AUDIO_FRAME:
            LOGD("mediaTransceiverCallback AUDIO codec=%d size=%u pts=%llu", codecID,
                    frame ? frame->size : 0,
                    static_cast<unsigned long long>(frame ? frame->presentationTs : 0));
            if (session->audioPlayback && frame && frame->frameData && frame->size > 0) {
                audioPlaybackEnqueueFrame(session->audioPlayback, frame->frameData, static_cast<int32_t>(frame->size));
            }
            break;
        case EASYRTC_TRANSCEIVER_CALLBACK_KEY_FRAME_REQ:
            LOGD("mediaTransceiverCallback KEY_FRAME_REQ bw=%.2f", bandwidthEstimation);
            if (session->videoEncoder && session->videoEncoder->encoder) {
                AMediaFormat* params = AMediaFormat_new();
                AMediaFormat_setInt32(params, "request-sync", 0);
                AMediaCodec_setParameters(session->videoEncoder->encoder, params);
                AMediaFormat_delete(params);
            }
            break;
        case EASYRTC_TRANSCEIVER_CALLBACK_BANDWIDTH:
            LOGD("mediaTransceiverCallback BANDWIDTH=%.2f", bandwidthEstimation);
            break;
        default:
            LOGD("mediaTransceiverCallback type=%d", type);
            break;
    }
    return 0;
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaSession_nativeCreate(JNIEnv* env, jobject thiz) {
    auto* session = new MediaSession();
    LOGD("MediaSession created");
    return reinterpret_cast<jlong>(session);
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStartPreview(
        JNIEnv* env, jobject thiz, jlong sessionPtr, jobject surface) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    assert(session && "Invalid session");
    assert(!session->previewRunning.load() && "Preview already running");
    assert(surface && "Invalid session");

    session->previewWindow = ANativeWindow_fromSurface(env, surface);

    ACameraManager* cameraMgr = ACameraManager_create();
    if (!cameraMgr) { ANativeWindow_release(session->previewWindow); session->previewWindow = nullptr; return -1; }

    std::string cameraId = findCameraId(session->cameraFacing);
    if (cameraId.empty()) {
        ANativeWindow_release(session->previewWindow); session->previewWindow = nullptr;
        ACameraManager_delete(cameraMgr);
        return -1;
    }

    ACameraDevice_StateCallbacks cb = {};
    cb.context = session;
    cb.onDisconnected = nullptr;
    cb.onError = nullptr;
    ACameraDevice* device = nullptr;
    camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb, &device);
    ACameraManager_delete(cameraMgr);

    if (camStatus != ACAMERA_OK || !device) {
        ANativeWindow_release(session->previewWindow); session->previewWindow = nullptr;
        return -1;
    }

    session->cameraDevice = device;
    if (!createCaptureSession(session, false)) {
        closeCamera(session);
        ANativeWindow_release(session->previewWindow); session->previewWindow = nullptr;
        return -1;
    }

    session->previewRunning.store(true);
    LOGD("Preview started");
    return 0;
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStopPreview(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    assert(session && "Invalid session");
    assert(!session->running.load() && "should call stop before stopPreview");

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
        JNIEnv* env, jobject thiz, jlong sessionPtr, jlong peerConnectionHandle) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    assert(session && "Invalid session");

    if (peerConnectionHandle == 0) {
        assert(!session->running.load() && "should call stop before resetting peerConnection");
    }

    session->peerConnection = reinterpret_cast<EasyRTC_PeerConnection>(peerConnectionHandle);
    session->transceiversAdded.store(false);
    LOGD("MediaSession peerConnection updated: %p", session->peerConnection);
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeAddTransceivers(
        JNIEnv* env, jobject thiz, jlong sessionPtr, jint videoCodec, jint audioCodec) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->peerConnection && "Invalid peerConnection");
    assert(videoCodec > 0 && audioCodec > 0 && "Invalid codec IDs");
    assert(!session->transceiversAdded.load() && "Transceivers already added");

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
    LOGD("Video transceiver added: %p", session->videoTransceiver);

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
    LOGD("Audio transceiver added: %p", session->audioTransceiver);

    session->transceiversAdded.store(true);

    if (session->videoEncoder && session->videoTransceiver) {
        session->videoEncoder->transceiver = session->videoTransceiver;
        LOGD("Video encoder transceiver wired: %p", session->videoTransceiver);
    }

    if (session->audioTransceiver && session->running.load() && !session->audioCapture) {
        session->audioCapture = audioCaptureCreate(session->audioTransceiver);
        audioCaptureStart(session->audioCapture);
    }

    return 0;
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetupVideoEncoder(
        JNIEnv* env, jobject thiz, jlong sessionPtr,
        jint codec, jint width, jint height, jint bitrate, jint fps, jint iframeInterval) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    assert(session && "Invalid session");
    assert(!session->running.load() && "should call stop before setupVideoEncoder");

    const char* mime = (codec == 6) ? "video/hevc" : "video/avc";
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, fps);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, iframeInterval);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatSurface);

    auto* pipeline = new MediaPipeline();
    pipeline->transceiver = session->videoTransceiver;
    pipeline->width = width;
    pipeline->height = height;
    pipeline->bitrate = bitrate;
    pipeline->fps = fps;
    pipeline->iframeInterval = iframeInterval;
    pipeline->format = format;

    AMediaCodec* encoder = AMediaCodec_createEncoderByType(mime);
    if (!encoder) {
        LOGE("Failed to create encoder for mime: %s", mime);
        AMediaFormat_delete(format);
        delete pipeline;
        return -1;
    }

    media_status_t status = AMediaCodec_configure(encoder, format, nullptr, nullptr,
            AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    if (status != AMEDIA_OK) {
        LOGE("Failed to configure encoder: %d", status);
        AMediaCodec_delete(encoder);
        AMediaFormat_delete(format);
        delete pipeline;
        return -1;
    }

    pipeline->encoder = encoder;

    ANativeWindow* inputWindow = nullptr;
    media_status_t surfStatus = AMediaCodec_createInputSurface(encoder, &inputWindow);
    if (surfStatus != AMEDIA_OK || !inputWindow) {
        LOGE("Failed to create input surface: %d", surfStatus);
        AMediaCodec_delete(encoder);
        AMediaFormat_delete(format);
        delete pipeline;
        return -1;
    }
    pipeline->window = inputWindow;

    session->videoEncoder = pipeline;
    LOGD("Video encoder setup: %dx%d @ %d bps, mime=%s", width, height, bitrate, mime);
    return 0;
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetPreviewSurface(
        JNIEnv* env, jobject thiz, jlong sessionPtr, jobject surface) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->videoEncoder && "Invalid video encoder");
    // no-op: preview surface is set via startPreview, encoder preview goes through capture session
    // kept for API compat but preview is managed by MediaSession's camera
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetDecoderSurface(
        JNIEnv* env, jobject thiz, jlong sessionPtr, jobject surface) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
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

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStart(JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    assert(session && "Invalid session");
    assert(!session->running.load() && "already running");

    if (session->videoEncoder) {
        auto* p = session->videoEncoder;
        media_status_t status = AMediaCodec_start(p->encoder);
        if (status != AMEDIA_OK) { LOGE("Failed to start encoder: %d", status); return -1; }

        p->running.store(true);
        p->outputThread = std::thread([p]() {
            outputThreadFunc(p);
        });

        if (session->cameraDevice) {
            std::lock_guard<std::mutex> lock(session->cameraMutex);
            releaseCaptureSession(session);
            if (!createCaptureSession(session, true)) {
                LOGE("Failed to recreate capture session with encoder");
            }
        }
        LOGD("Video encoder started");
    }

    if (session->audioTransceiver) {
        session->audioCapture = audioCaptureCreate(session->audioTransceiver);
        audioCaptureStart(session->audioCapture);
    }

    session->audioPlayback = audioPlaybackCreate();

    if (session->decoderSurface) {
        session->videoDecoder = videoDecoderCreate(session->decoderSurface, session->videoCodec, 720, 1280);
        if (session->videoDecoder) { videoDecoderStart(session->videoDecoder); }
        session->decoderSurface = nullptr;
    }

    session->running.store(true);
    LOGD("MediaSession started");
    return 0;
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStop(JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    assert(session && "Invalid session");

    session->running.store(false);

    if (session->audioCapture) { audioCaptureRelease(session->audioCapture); session->audioCapture = nullptr; }
    if (session->audioPlayback) { audioPlaybackRelease(session->audioPlayback); session->audioPlayback = nullptr; }
    if (session->videoDecoder) { videoDecoderRelease(session->videoDecoder); session->videoDecoder = nullptr; }

    if (session->videoEncoder) {
        auto* p = session->videoEncoder;
        if (p->running.exchange(false)) {
            if (p->encoder) { AMediaCodec_signalEndOfInputStream(p->encoder); }
            if (p->outputThread.joinable()) { p->outputThread.join(); }
            if (p->encoder) { AMediaCodec_stop(p->encoder); }
        }
        if (p->encoder) { AMediaCodec_delete(p->encoder); p->encoder = nullptr; }
        if (p->format) { AMediaFormat_delete(p->format); p->format = nullptr; }
        if (p->window) { ANativeWindow_release(p->window); p->window = nullptr; }
        {
            std::lock_guard<std::recursive_mutex> lock(p->mutex);
            delete[] p->sps_pps_buffer;
            p->sps_pps_buffer = nullptr;
            p->sps_pps_size = 0;
        }
        delete p;
        session->videoEncoder = nullptr;
    }

    if (session->cameraDevice) {
        std::lock_guard<std::mutex> lock(session->cameraMutex);
        releaseCaptureSession(session);
        if (session->previewWindow) {
            createCaptureSession(session, false);
        }
    }

    LOGD("MediaSession stopped");
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSwitchCamera(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    assert(session && "Invalid session");

    std::lock_guard<std::mutex> lock(session->cameraMutex);
    releaseCaptureSession(session);
    if (session->cameraDevice) {
        ACameraDevice_close(session->cameraDevice);
        session->cameraDevice = nullptr;
    }

    session->cameraFacing = (session->cameraFacing == 0) ? 1 : 0;

    ACameraManager* cameraMgr = ACameraManager_create();
    if (!cameraMgr) { return; }

    std::string cameraId = findCameraId(session->cameraFacing);
    if (!cameraId.empty()) {
        ACameraDevice_StateCallbacks cb = {};
        cb.context = session;
        cb.onDisconnected = nullptr;
        cb.onError = nullptr;
        ACameraDevice* device = nullptr;
        camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb, &device);
        if (camStatus == ACAMERA_OK && device) {
            session->cameraDevice = device;
            createCaptureSession(session, session->running.load());
        }
    }
    ACameraManager_delete(cameraMgr);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeRequestKeyFrame(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session || !session->videoEncoder || !session->videoEncoder->encoder) return;
    AMediaFormat* params = AMediaFormat_new();
    AMediaFormat_setInt32(params, "request-sync", 0);
    AMediaCodec_setParameters(session->videoEncoder->encoder, params);
    AMediaFormat_delete(params);
}

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaSession_nativeGetVideoTransceiver(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    return session ? reinterpret_cast<jlong>(session->videoTransceiver) : 0;
}

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaSession_nativeGetAudioTransceiver(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    return session ? reinterpret_cast<jlong>(session->audioTransceiver) : 0;
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeRelease(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session) return;

    Java_cn_easyrtc_media_MediaSession_nativeStop(env, thiz, sessionPtr);

    closeCamera(session);
    if (session->previewWindow) { ANativeWindow_release(session->previewWindow); session->previewWindow = nullptr; }
    session->previewRunning.store(false);

    if (session->videoTransceiver) { EasyRTC_FreeTransceiver(&session->videoTransceiver); session->videoTransceiver = nullptr; }
    if (session->audioTransceiver) { EasyRTC_FreeTransceiver(&session->audioTransceiver); session->audioTransceiver = nullptr; }
    session->transceiversAdded.store(false);

    if (session->decoderSurface) { ANativeWindow_release(session->decoderSurface); session->decoderSurface = nullptr; }

    delete session;
    LOGD("MediaSession released");
}

}
