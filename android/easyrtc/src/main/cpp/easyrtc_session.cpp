#include "easyrtc_session.h"
#include "easyrtc_media.h"
#include "easyrtc_audio.h"
#include "easyrtc_audio_playback.h"
#include "easyrtc_video_decoder.h"
#include <jni.h>
#include <cstring>

static int mediaTransceiverCallback(void* userPtr,
        EASYRTC_TRANSCEIVER_CALLBACK_TYPE_E type,
        EasyRTC_CODEC codecID,
        EasyRTC_Frame* frame,
        double bandwidthEstimation) {

    auto* session = static_cast<MediaSession*>(userPtr);
    if (!session) {
        return -1;
    }

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
Java_cn_easyrtc_media_MediaSession_nativeCreate(
        JNIEnv* env, jobject thiz, jlong peerConnectionHandle) {
    auto* session = new MediaSession();
    session->peerConnection = reinterpret_cast<EasyRTC_PeerConnection>(peerConnectionHandle);
    LOGD("MediaSession created: peerConnection=%p", session->peerConnection);
    return reinterpret_cast<jlong>(session);
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeAddTransceivers(
        JNIEnv* env, jobject thiz, jlong sessionPtr,
        jint videoCodec, jint audioCodec) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session || !session->peerConnection) {
        return -1;
    }

    if (session->transceiversAdded.load()) {
        LOGD("nativeAddTransceivers skip: already added video=%p audio=%p", session->videoTransceiver, session->audioTransceiver);
        return 0;
    }

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
        return -2;
    }
    LOGD("Audio transceiver added: %p", session->audioTransceiver);

    session->transceiversAdded.store(true);

    return 0;
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetupVideoEncoder(
        JNIEnv* env, jobject thiz, jlong sessionPtr,
        jint codec, jint width, jint height, jint bitrate, jint fps, jint iframeInterval) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session) {
        return -1;
    }

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
    session->videoEncoder = pipeline;
    LOGD("Video encoder setup: %dx%d @ %d bps, mime=%s", width, height, bitrate, mime);
    return 0;
}

JNIEXPORT jobject JNICALL
Java_cn_easyrtc_media_MediaSession_nativeCreateEncoderSurface(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session || !session->videoEncoder || !session->videoEncoder->encoder) {
        return nullptr;
    }

    ANativeWindow* window = nullptr;
    media_status_t status = AMediaCodec_createInputSurface(session->videoEncoder->encoder, &window);
    if (status != AMEDIA_OK || !window) {
        LOGE("Failed to create input surface: %d", status);
        return nullptr;
    }

    session->videoEncoder->window = window;
    return ANativeWindow_toSurface(env, window);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetPreviewSurface(
        JNIEnv* env, jobject thiz, jlong sessionPtr, jobject surface) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session || !session->videoEncoder) {
        return;
    }

    if (session->videoEncoder->previewWindow) {
        ANativeWindow_release(session->videoEncoder->previewWindow);
        session->videoEncoder->previewWindow = nullptr;
    }

    if (surface) {
        session->videoEncoder->previewWindow = ANativeWindow_fromSurface(env, surface);
    }
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetDecoderSurface(
        JNIEnv* env, jobject thiz, jlong sessionPtr, jobject surface) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session) {
        return;
    }

    if (session->decoderSurface) {
        ANativeWindow_release(session->decoderSurface);
        session->decoderSurface = nullptr;
    }

    if (surface) {
        session->decoderSurface = ANativeWindow_fromSurface(env, surface);
    }
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStart(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session) {
        return -1;
    }

    if (session->videoEncoder) {
        auto* p = session->videoEncoder;
        media_status_t status = AMediaCodec_start(p->encoder);
        if (status != AMEDIA_OK) {
            LOGE("Failed to start encoder: %d", status);
            return -1;
        }

        ACameraManager* cameraMgr = ACameraManager_create();
        if (cameraMgr) {
            std::string cameraId = findCameraId(p->cameraFacing);
            if (!cameraId.empty()) {
                ACameraDevice_StateCallbacks cb = {};
                cb.context = p;
                cb.onDisconnected = nullptr;
                cb.onError = nullptr;
                ACameraDevice* device = nullptr;
                camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb, &device);
                if (camStatus == ACAMERA_OK && device) {
                    p->cameraDevice = device;
                    startCameraCapture(p);
                }
            }
            ACameraManager_delete(cameraMgr);
        }

        p->running.store(true);
        pthread_create(&p->output_thread, nullptr, outputThreadFunc, p);
        LOGD("Video encoder started");
    }

    if (session->audioTransceiver) {
        session->audioCapture = audioCaptureCreate(session->audioTransceiver);
        audioCaptureStart(session->audioCapture);
    }

    session->audioPlayback = audioPlaybackCreate();

    if (session->decoderSurface) {
        session->videoDecoder = videoDecoderCreate(session->decoderSurface, session->videoCodec, 720, 1280);
        if (session->videoDecoder) {
            videoDecoderStart(session->videoDecoder);
        }
        session->decoderSurface = nullptr;
    }

    session->running.store(true);
    LOGD("MediaSession started");
    return 0;
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStop(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session) {
        return;
    }

    session->running.store(false);

    if (session->audioCapture) {
        audioCaptureRelease(session->audioCapture);
        session->audioCapture = nullptr;
    }

    if (session->audioPlayback) {
        audioPlaybackRelease(session->audioPlayback);
        session->audioPlayback = nullptr;
    }

    if (session->videoDecoder) {
        videoDecoderRelease(session->videoDecoder);
        session->videoDecoder = nullptr;
    }

    if (session->videoEncoder) {
        auto* p = session->videoEncoder;
        if (p->running.exchange(false)) {
            closeCamera(p);
            if (p->encoder) {
                AMediaCodec_signalEndOfInputStream(p->encoder);
            }
            if (p->output_thread) {
                pthread_join(p->output_thread, nullptr);
                p->output_thread = 0;
            }
            if (p->encoder) {
                AMediaCodec_stop(p->encoder);
            }
        }

        closeCamera(p);
        if (p->encoder) {
            AMediaCodec_delete(p->encoder);
            p->encoder = nullptr;
        }
        if (p->format) {
            AMediaFormat_delete(p->format);
            p->format = nullptr;
        }
        if (p->window) {
            ANativeWindow_release(p->window);
            p->window = nullptr;
        }
        if (p->previewWindow) {
            ANativeWindow_release(p->previewWindow);
            p->previewWindow = nullptr;
        }
        pthread_mutex_lock(&p->mutex);
        delete[] p->sps_pps_buffer;
        p->sps_pps_buffer = nullptr;
        p->sps_pps_size = 0;
        pthread_mutex_unlock(&p->mutex);

        delete p;
        session->videoEncoder = nullptr;
    }

    LOGD("MediaSession stopped");
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSwitchCamera(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session || !session->videoEncoder) {
        return;
    }

    auto* p = session->videoEncoder;
    if (!p->running.load()) {
        return;
    }

    closeCamera(p);
    p->cameraFacing = (p->cameraFacing == 0) ? 1 : 0;

    ACameraManager* cameraMgr = ACameraManager_create();
    if (!cameraMgr) {
        return;
    }

    std::string cameraId = findCameraId(p->cameraFacing);
    if (!cameraId.empty()) {
        ACameraDevice_StateCallbacks cb = {};
        cb.context = p;
        cb.onDisconnected = nullptr;
        cb.onError = nullptr;
        ACameraDevice* device = nullptr;
        camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb, &device);
        if (camStatus == ACAMERA_OK && device) {
            p->cameraDevice = device;
            startCameraCapture(p);
        }
    }
    ACameraManager_delete(cameraMgr);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeRequestKeyFrame(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session || !session->videoEncoder || !session->videoEncoder->encoder) {
        return;
    }

    AMediaFormat* params = AMediaFormat_new();
    AMediaFormat_setInt32(params, "request-sync", 0);
    AMediaCodec_setParameters(session->videoEncoder->encoder, params);
    AMediaFormat_delete(params);
}

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaSession_nativeGetVideoTransceiver(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session) {
        return 0;
    }
    LOGD("nativeGetVideoTransceiver: %p", session->videoTransceiver);
    return reinterpret_cast<jlong>(session->videoTransceiver);
}

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_MediaSession_nativeGetAudioTransceiver(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session) {
        return 0;
    }
    LOGD("nativeGetAudioTransceiver: %p", session->audioTransceiver);
    return reinterpret_cast<jlong>(session->audioTransceiver);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_MediaSession_nativeRelease(
        JNIEnv* env, jobject thiz, jlong sessionPtr) {
    auto* session = reinterpret_cast<MediaSession*>(sessionPtr);
    if (!session) {
        return;
    }

    Java_cn_easyrtc_media_MediaSession_nativeStop(env, thiz, sessionPtr);

    if (session->videoTransceiver) {
        EasyRTC_FreeTransceiver(&session->videoTransceiver);
        session->videoTransceiver = nullptr;
    }
    if (session->audioTransceiver) {
        EasyRTC_FreeTransceiver(&session->audioTransceiver);
        session->audioTransceiver = nullptr;
    }
    session->transceiversAdded.store(false);

    if (session->decoderSurface) {
        ANativeWindow_release(session->decoderSurface);
        session->decoderSurface = nullptr;
    }

    delete session;
    LOGD("MediaSession released");
}

}
