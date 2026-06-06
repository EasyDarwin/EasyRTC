#include "session/media_session.h"
#include "session/common.h"
#include "session/camera_error_handler.h"
#include "session/transceiver_stats_reporter.h"
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
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

namespace {
    // std::atomic<uint64_t> gVideoCbCount{0};
    // std::atomic<uint64_t> gVideoCbBytes{0};
    // std::atomic<uint64_t> gVideoCbAnnexB{0};
    // std::atomic<uint64_t> gVideoCbAvcc{0};

}

// ─── Early EGL / OES texture creation (before GL bridge exists) ────────────

static void ensureCameraInputSurfaceTexture(JNIEnv *env, MediaSession *session, int width, int height);

static void createEarlyEglAndOesTex(MediaSession* s) {
    assert(s->cameraOesTex == 0 && "Early EGL and OES texture should only be created once");
    bool ok = true;
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("[CRITICAL] createEarlyEgl: eglGetDisplay failed");
        assert(false && "eglGetDisplay failed");
        return;
    }
    if (!eglInitialize(display, nullptr, nullptr)) {
        LOGE("[CRITICAL] createEarlyEgl: eglInitialize failed");
        assert(false && "eglInitialize failed");
        return;
    }

    const EGLint configAttrs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint numConfig = 0;
    if (!eglChooseConfig(display, configAttrs, &config, 1, &numConfig) || numConfig < 1) {
        LOGE("[CRITICAL] createEarlyEgl: eglChooseConfig failed");
        eglTerminate(display);
        assert(false && "eglChooseConfig failed");
        return;
    }

    const EGLint pbufferAttrs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    EGLSurface pbuffer = eglCreatePbufferSurface(display, config, pbufferAttrs);
    if (pbuffer == EGL_NO_SURFACE) {
        LOGE("[CRITICAL] createEarlyEgl: eglCreatePbufferSurface failed");
        eglTerminate(display);
        assert(false && "eglCreatePbufferSurface failed");
        return;
    }

    const EGLint ctxAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttrs);
    if (context == EGL_NO_CONTEXT) {
        LOGE("[CRITICAL] createEarlyEgl: eglCreateContext failed");
        eglDestroySurface(display, pbuffer);
        eglTerminate(display);
        assert(false && "eglCreateContext failed");
        return;
    }

    if (!eglMakeCurrent(display, pbuffer, pbuffer, context)) {
        LOGE("[CRITICAL] createEarlyEgl: eglMakeCurrent failed");
        eglDestroyContext(display, context);
        eglDestroySurface(display, pbuffer);
        eglTerminate(display);
        assert(false && "eglMakeCurrent failed");
        return;
    }

    glGenTextures(1, &s->cameraOesTex);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, s->cameraOesTex);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    assert(s->cameraOesTex != 0 && "Failed to create OES texture for camera input");

    // Don't keep the pbuffer surface current; just keep context + OES tex alive
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, pbuffer);

    s->cameraEglDisplay = reinterpret_cast<void*>(display);
    s->cameraEglContext = reinterpret_cast<void*>(context);
    LOGI("[CRITICAL] createEarlyEgl: SUCCESS display=%p ctx=%p oesTex=%u",
         s->cameraEglDisplay, s->cameraEglContext, s->cameraOesTex);

}

static void releaseEarlyEgl(MediaSession* s) {
    if (s->cameraOesTex != 0) {
        EGLDisplay display = reinterpret_cast<EGLDisplay>(s->cameraEglDisplay);
        EGLContext context = reinterpret_cast<EGLContext>(s->cameraEglContext);
        if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
            glDeleteTextures(1, &s->cameraOesTex);
            eglDestroyContext(display, context);
            eglTerminate(display);
            LOGI("[CRITICAL] releaseEarlyEgl: done oesTex=%u", s->cameraOesTex);
        }
    }
    s->cameraOesTex = 0;
    s->cameraEglDisplay = nullptr;
    s->cameraEglContext = nullptr;
    s->cameraInputReady = false;
}

// ─── Capture request builders ─────────────────────────────────────────────

static ACaptureRequest* buildRequestWithTargets(MediaSession* s, bool includeEncoder) {
    if (!s->cameraDevice) return nullptr;

    ACaptureRequest* req = nullptr;
    ACameraDevice_createCaptureRequest(s->cameraDevice, TEMPLATE_PREVIEW, &req);
    if (!req) return nullptr;

    if (includeEncoder && s->cameraInputTarget) {
        ACaptureRequest_addTarget(req, s->cameraInputTarget);
    }
    if (s->previewTarget) {
        ACaptureRequest_addTarget(req, s->previewTarget);
    }
    uint8_t afMode = ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
    ACaptureRequest_setEntry_u8(req, ACAMERA_CONTROL_AF_MODE, 1, &afMode);
    return req;
}

static bool switchRepeatingRequest(MediaSession* s, bool includeEncoder) {
    if (!s->captureSession) return false;
    auto* old = s->captureRequest;
    s->captureRequest = buildRequestWithTargets(s, includeEncoder);
    if (!s->captureRequest) {
        s->captureRequest = old;
        return false;
    }
    camera_status_t status = ACameraCaptureSession_setRepeatingRequest(
            s->captureSession, nullptr, 1, &s->captureRequest, nullptr);
    if (status != ACAMERA_OK) {
        LOGE("[CRITICAL] switchRepeatingRequest: setRepeatingRequest failed %d", status);
        return false;
    }
    if (old) ACaptureRequest_free(old);
    LOGI("[CRITICAL] switchRepeatingRequest: encoder=%d", includeEncoder ? 1 : 0);
    return true;
}

// ─── Camera error callbacks (diagnostic logging) ──────────────────────────

static void releaseCaptureSessionOutputs(MediaSession *s);
static void releaseEarlyEgl(MediaSession* s);

static void onSessionActive(void* context, ACameraCaptureSession* session) {
    auto* s = static_cast<MediaSession*>(context);
    LOGI("[CAMERA SESSION] ACTIVE: session=%p", s);
}

static void onSessionReady(void* context, ACameraCaptureSession* session) {
    auto* s = static_cast<MediaSession*>(context);
    LOGI("[CAMERA SESSION] READY: session=%p", s);
}

static void onSessionClosed(void* context, ACameraCaptureSession* session) {
    auto* s = static_cast<MediaSession*>(context);
    LOGW("[CAMERA SESSION] CLOSED: session=%p", s);
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
    if (s->captureSession) {
        ACameraCaptureSession_stopRepeating(s->captureSession);
        s->captureSession = nullptr;
    }
    if (s->cameraDevice) {
        ACameraDevice_close(s->cameraDevice);
        s->cameraDevice = nullptr;
    }
    if (s->cameraInputSurfaceTexture) {
        ASurfaceTexture_release(s->cameraInputSurfaceTexture);
        s->cameraInputSurfaceTexture = nullptr;
    }
    if (s->cameraInputSurfaceTextureObj) {
        JNIEnv* env = nullptr;
        bool attached = false;
        if (s->jvm && s->jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (s->jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
        }
        if (env) {
            env->DeleteGlobalRef(s->cameraInputSurfaceTextureObj);
            if (attached) s->jvm->DetachCurrentThread();
        }
        s->cameraInputSurfaceTextureObj = nullptr;
    }
    s->cameraInputWindow.reset();
    releaseCaptureSessionOutputs(s);
    releaseEarlyEgl(s);
}

static bool createCaptureSession(MediaSession *s, bool withEncoder) {
    LOGI("[CRITICAL] createCaptureSession ENTRY: s=%p withEncoder=%d", s, withEncoder ? 1 : 0);
    camera_status_t camStatus;

    camStatus = ACaptureSessionOutputContainer_create(&s->outputContainer);
    if (camStatus != ACAMERA_OK) {
        releaseCaptureSession(s);
        return false;
    }
    assert(s->cameraInputWindow && "cameraInputWindow should be created before capture session");
    {
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
        camStatus = ACameraOutputTarget_create(
                reinterpret_cast<ACameraWindowType *>(s->cameraInputWindow.get()), &s->cameraInputTarget);
        if (camStatus != ACAMERA_OK) {
            releaseCaptureSession(s);
            return false;
        }
        s->cameraInputReady = true;
    }
    assert(s->previewWindow && "previewWindow should be created before capture session");
    {
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
        camStatus = ACameraOutputTarget_create(
                reinterpret_cast<ACameraWindowType *>(s->previewWindow.get()), &s->previewTarget);
        if (camStatus != ACAMERA_OK) {
            releaseCaptureSession(s);
            return false;
        }
    }

    // Build initial request (preview-only by default, encoder target added when needed)
    s->captureRequest = buildRequestWithTargets(s, withEncoder);
    if (!s->captureRequest) {
        releaseCaptureSession(s);
        return false;
    }

    const ACameraCaptureSession_stateCallbacks sessionCallbacks = {
            s, onSessionClosed, onSessionReady, onSessionActive
    };
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
                videoDecoderStart(session);
            }

            if (session->videoDecoder && frame && frame->frameData && frame->size > 0) {
                
                frameDumpWrite(&session->frameDump, FrameDumpWriter::KIND_VIDEO, frame->frameData,
                               frame->size, frame->flags);
                if (session->statsReporter) {
                    session->statsReporter->accumulateVideoBytes(frame->size);
                }
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
                if (session->statsReporter) {
                    session->statsReporter->accumulateAudioBytes(frame->size);
                }
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
    if (strlen(sdp) > 0) {
        LOGI("[CRITICAL] %s content:\n%s", isOffer ? "Offer" : "Answer", sdp);
    }
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
    session->cameraError.store(0);

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
    if (0)
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
    cb.onDisconnected = onCameraDeviceDisconnected;
    cb.onError = onCameraDeviceError;
    ACameraDevice *device = nullptr;
    camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb,
                                                          &device);
    if (camStatus != ACAMERA_OK || !device) {
        LOGE("[CRITICAL] nativeStartPreview: openCamera failed: %d", camStatus);
        return -1;
    }

    session->cameraDevice = device;

    // Create early EGL context + OES texture + SurfaceTexture for camera-to-encoder path.
    // This allows the capture session to include the cameraInput output from the start,
    // avoiding session rebuild when encoding starts/stops.
    assert(session->cameraOesTex == 0 && "cameraOesTex should be 0 before creation");
    assert(session->cameraInputSurfaceTexture == nullptr && "cameraInputSurfaceTexture should be null before creation");
    assert(session->cameraInputWindow == nullptr && "cameraInputWindow should be null before creation");
    assert(session->encoderParams.width > 0 && session->encoderParams.height > 0 && "Encoder params should be set before starting preview");
    int stW = session->encoderSwapWH ? session->encoderParams.height : session->encoderParams.width;
    int stH = session->encoderSwapWH ? session->encoderParams.width : session->encoderParams.height;
    createEarlyEglAndOesTex(session);
    ensureCameraInputSurfaceTexture(env, session, stW, stH);

    if (!createCaptureSession(session, false)) {
        assert(false && "Failed to create camera capture session");
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

    session->statsReporter = std::make_shared<TransceiverStatsReporter>(session);

    session->audioPlaybackInitAttempted = false;
    session->videoDecoderInitAttempted = false;
    return 0;
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeSetupVideoEncoderParam(
        JNIEnv *env, jobject thiz, jlong sessionPtr,
        jint codec, jint width, jint height, jint bitrate, jint fps, jint iframeInterval) {
    LOGI("[CRITICAL] nativeSetupVideoEncoderParam ENTRY: sessionPtr=%p codec=%d size=%dx%d bitrate=%d fps=%d iframe=%d",
         reinterpret_cast<void *>(sessionPtr), codec, width, height, bitrate, fps, iframeInterval);
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");

    const char *mime = (codec == EasyRTC_CODEC_H265) ? "video/hevc" : "video/avc";
    const bool swapWH = session->encoderSwapWH;
    const int encoderWidth = swapWH ? height : width;
    const int encoderHeight = swapWH ? width : height;

    session->videoCodec = codec;
    session->encoderParams.width = encoderWidth;
    session->encoderParams.height = encoderHeight;
    session->encoderParams.bitrate = bitrate;
    session->encoderParams.fps = fps;
    session->encoderParams.iframeInterval = iframeInterval;
    session->encoderParams.mime = mime;

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

static void ensureCameraInputSurfaceTexture(JNIEnv *env, MediaSession *session, int width, int height) {
    assert(session && "Invalid session");
    assert(env && "Invalid JNI environment");
    assert(session->cameraOesTex != 0 && "cameraOesTex should be created before ensuring SurfaceTexture");
    assert(session->javaObj && "Java object should be set before ensuring SurfaceTexture");
    assert(session->cameraInputWindow == nullptr &&
           "Camera input cameraInputWindow should not exist before ensuring");
    assert(session->cameraInputSurfaceTexture == nullptr &&
           "Camera input cameraInputSurfaceTexture should not exist before ensuring");
    assert(session->cameraInputSurfaceTextureObj == nullptr &&
           "Camera input SurfaceTexture Java object should not exist before ensuring");
    jclass clazz = env->GetObjectClass(session->javaObj);
    assert(clazz && "Failed to get Java class for creating SurfaceTexture");
    jmethodID mid = env->GetMethodID(clazz, "createCameraInputSurfaceTexture", "(III)Landroid/graphics/SurfaceTexture;");
    assert(mid && "Failed to get method ID for createCameraInputSurfaceTexture");
    jobject stLocal = env->CallObjectMethod(session->javaObj, mid,
                                            static_cast<jint>(session->cameraOesTex),
                                            static_cast<jint>(width), static_cast<jint>(height));
    assert(stLocal && "createCameraInputSurfaceTexture returned null");
    assert(env->IsInstanceOf(stLocal, env->FindClass("android/graphics/SurfaceTexture")) &&
           "Returned object is not an instance of SurfaceTexture");

    session->cameraInputSurfaceTextureObj = env->NewGlobalRef(stLocal);
    env->DeleteLocalRef(stLocal);
    session->cameraInputSurfaceTexture = ASurfaceTexture_fromSurfaceTexture(env, session->cameraInputSurfaceTextureObj);
    assert(session->cameraInputSurfaceTexture && "Failed to create ASurfaceTexture from SurfaceTexture");
    session->cameraInputWindow = std::shared_ptr<ANativeWindow>(ASurfaceTexture_acquireANativeWindow(session->cameraInputSurfaceTexture), ANativeWindow_release);
    assert(session->cameraInputWindow && "Failed to acquire ANativeWindow from ASurfaceTexture");
    LOGI("[CRITICAL] EncoderRotate camera input ST created: st=%p window=%p tex=%u bufferSize=%dx%d",
         session->cameraInputSurfaceTexture, session->cameraInputWindow.get(),
         session->cameraOesTex, width, height);
}

static void startRenderThread(MediaSession *session) {
    auto encoder = session->videoEncoder;
    assert(encoder && "Video encoder must be initialized before starting render thread");
        // Pass early EGL context + OES tex to GL bridge so it reuses them
    session->encoderGlBridge = encoderGlCreate(
            encoder->encoderInputSurface.get(), encoder->width, encoder->height, session->encoderRotation,
            session->cameraOesTex,
            session->cameraEglDisplay, session->cameraEglContext);

    if (!session->deviceId.empty() && session->encoderGlBridge) {
        session->encoderGlBridge->deviceId = session->deviceId;
    }
    assert (session->encoderGlBridge);
    LOGI("[CRITICAL] Render start: rotation=%d swapWH=%d", session->encoderRotation, session->encoderSwapWH ? 1 : 0);
    const uint32_t frameIntervalUs = static_cast<uint32_t>(1000000 / (encoder->fps > 0 ? encoder->fps : 29.97));
    assert(session->renderThreadRunning.load() == false && "Render thread already running");
    session->renderThreadRunning.store(true);
    session->renderThread = std::thread([session, frameIntervalUs]() {
        LOGI("[Render] thread started");
        defer(LOGI("[Render] thread stopped"));
        if (!encoderGlMakeCurrent(session->encoderGlBridge)) {
            LOGE("[Render] thread makeCurrent failed");
            assert(false && "Failed to make GL context current in render thread");
        }
        assert(session->cameraInputSurfaceTexture && "cameraInputSurfaceTexture invalid");
        assert(session->encoderGlBridge->cameraOesTex && "cameraOesTex invalid");
        int attach = ASurfaceTexture_attachToGLContext(session->cameraInputSurfaceTexture, session->encoderGlBridge->cameraOesTex);
        LOGI("[Render] thread: attachToGLContext result=%d", attach);
//        assert(attach == 0 && "Failed to attach SurfaceTexture to GL context in render thread");
        while (session->renderThreadRunning.load() && !session->shuttingDown.load()) {
            float m[16];
                int64_t ts = 0;
                if (encoderGlUpdateTexImage(session->cameraInputSurfaceTexture, m, &ts)) {
                    // Keep SurfaceTexture drained, but only feed encoder when connected.
                    if (session->connectState == 3) {
                        encoderGlSetInputTransform(session->encoderGlBridge, m);
                        if (!encoderGlRenderFrame(session->encoderGlBridge, static_cast<long long>(ts))) {
                            LOGW("[Render] thread frame render failed");
                            assert(false && "Encoder render failed");
                        }
                    } else {
                        FLOGI("[Render] thread: not connected, skipping frame");
                    }
                }else {
                    // LOGE("[Render] thread updateTexImage failed");
                    // assert(false && "Encoder updateTexImage failed");
                }
            usleep(frameIntervalUs);
        }
        assert (session->cameraInputSurfaceTexture);
        LOGI("[Render] thread: detaching SurfaceTexture");
        ASurfaceTexture_detachFromGLContext(session->cameraInputSurfaceTexture);
    });
    pthread_t th = session->renderThread.native_handle();
    sched_param sch_params;
    sch_params.sched_priority = 0;//sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(th, SCHED_FIFO, &sch_params);
}

static void stopRenderThread(MediaSession *session) {
    assert (session && "Invalid session");
    LOGI("[CRITICAL] stopRenderThread ENTRY: session=%p running=%d", session, (session->renderThreadRunning.load() ? 1 : 0));
    
    session->renderThreadRunning.store(false);
    if (session->renderThread.joinable()) {
        session->renderThread.join();
    }
    LOGI("[CRITICAL] nativeStopSend: before encoderGlRelease");
    encoderGlReleaseKeepOesEgl(session->encoderGlBridge);
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_MediaSession_nativeStartSend(JNIEnv *env, jobject thiz, jlong sessionPtr) {
    LOGI("[CRITICAL] nativeStartSend ENTRY: sessionPtr=%p", reinterpret_cast<void *>(sessionPtr));
    auto *session = reinterpret_cast<MediaSession *>(sessionPtr);
    assert(session && "Invalid session");
    assert(session->encoderParams.width > 0 && "encoder params not set");
    assert(session->videoTransceiver && "video transceiver not initialized");

    auto pipeline = std::make_shared<MediaPipeline>();
    pipeline->transceiver = session->videoTransceiver;
    pipeline->width = session->encoderParams.width;
    pipeline->height = session->encoderParams.height;
    pipeline->bitrate = session->encoderParams.bitrate;
    pipeline->fps = session->encoderParams.fps;
    pipeline->iframeInterval = session->encoderParams.iframeInterval;
    pipeline->mime = session->encoderParams.mime;

    AMediaFormat *format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, pipeline->mime.c_str());
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, pipeline->width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, pipeline->height);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, pipeline->bitrate);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BITRATE_MODE, 2);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, pipeline->fps);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, pipeline->iframeInterval);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatSurface);
    pipeline->format = std::shared_ptr<AMediaFormat>(format, AMediaFormat_delete);

    session->videoEncoder = pipeline;
    assert(!session->videoEncoder->running.load() && "videoEncoder already running");
    LOGI("[CRITICAL] StartSend: initializing video encoder");
    auto p = session->videoEncoder;
    if (!p->initEncoder(session)) {
        LOGE("[CRITICAL] StartSend: initEncoder FAILED");
        return -1;
    }
    assert(p->encoder);

    startRenderThread(session);
    media_status_t status = AMediaCodec_start(p->encoder);
    if (status != AMEDIA_OK) {
        LOGE("Failed to start encoder: %d", status);
        return -1;
    }
    p->startEncoder(session);

    // Switch repeating request to include encoder target (no session rebuild)
    if (session->cameraDevice && session->cameraInputReady) {
        switchRepeatingRequest(session, true);
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

    if (session->captureSession) {
        ACameraCaptureSession_stopRepeating(session->captureSession);
        session->captureSession = nullptr;
    }
    if (session->cameraDevice) {
        ACameraDevice_close(session->cameraDevice);
        session->cameraDevice = nullptr;
    }
    releaseCaptureSessionOutputs(session);
    auto rendering = session->renderThreadRunning.load();
    session->cameraFacing = (session->cameraFacing == 0) ? 1 : 0;

    ACameraManager *cameraMgr = ACameraManager_create();
    if (!cameraMgr) { return; }

    std::string cameraId = findCameraId(session->cameraFacing);
    if (!cameraId.empty()) {
        ACameraDevice_StateCallbacks cb = {};
        cb.context = session;
        cb.onDisconnected = onCameraDeviceDisconnected;
        cb.onError = onCameraDeviceError;
        ACameraDevice *device = nullptr;
        camera_status_t camStatus = ACameraManager_openCamera(cameraMgr, cameraId.c_str(), &cb,
                                                              &device);
        if (camStatus == ACAMERA_OK && device) {
            session->cameraDevice = device;
            createCaptureSession(session,rendering);
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
    session->shuttingDown.store(true);
    session->transceiversAdded.store(false);

    // 1. Stop send pipeline (render thread, encoder, audio capture)
    LOGI("[CRITICAL] StopSend: stopping audio capture and video encoder");
    audioCaptureStop(session);
    LOGI("[CRITICAL] nativeStopSend: before stopRenderThread");
    stopRenderThread(session);

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
    switchRepeatingRequest(session, false);
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
    session->statsReporter.reset();

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
    assert(session->audioTransceiver);
    assert(session->videoTransceiver);
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
    LOGI("ICE stats: local=%d remote=%d state=%d nominated=%d sent=%llu recv=%llu discarded=%u",
         stats.local_iceCandidateType, stats.remote_iceCandidateType,
         stats.state, stats.nominated,
         static_cast<unsigned long long>(stats.packetsSent),
         static_cast<unsigned long long>(stats.packetsReceived),
         stats.packetsDiscardedOnSend);
    bool relayed = (stats.local_iceCandidateType == EasyRTC_ICE_CANDIDATE_TYPE_RELAYED ||
                    stats.remote_iceCandidateType == EasyRTC_ICE_CANDIDATE_TYPE_RELAYED);
    return static_cast<jint>(relayed ? 2 : 1);
}

JNIEXPORT jstring JNICALL
Java_cn_easyrtc_media_MediaSession_00024Companion_nativeGetVersion(
        JNIEnv *env, jclass clazz) {
    char version[128] = {0};
    int ret = EasyRTC_GetVersion(version);
    if (ret != 0) {
        return env->NewStringUTF("unknown");
    }
    return env->NewStringUTF(version);
}

}
