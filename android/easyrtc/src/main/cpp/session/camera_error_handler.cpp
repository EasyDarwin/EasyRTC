#include "session/camera_error_handler.h"
#include "session/media_session.h"
#include "session/common.h"
#include <jni.h>

void notifyCameraError(MediaSession *s, int error) {
    if (!s || !s->jvm || !s->javaObj) {
        return;
    }
    JNIEnv *env = nullptr;
    bool attached = false;
    int ret = s->jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    if (ret != JNI_OK) {
        if (s->jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            attached = true;
        } else {
            return;
        }
    }

    jclass clazz = env->GetObjectClass(s->javaObj);
    if (clazz) {
        jmethodID mid = env->GetMethodID(clazz, "onCameraError", "(I)V");
        if (mid) {
            env->CallVoidMethod(s->javaObj, mid, static_cast<jint>(error));
        }
    }

    if (attached) {
        s->jvm->DetachCurrentThread();
    }
}

std::string queryEncoderForSurfaceInput(MediaSession *s, const std::string &mime) {
    std::string result;
    if (!s || !s->jvm || !s->javaObj) return result;
    JNIEnv *env = nullptr;
    bool attached = false;
    if (s->jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (s->jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return result;
        attached = true;
    }

    jclass clazz = env->GetObjectClass(s->javaObj);
    if (clazz) {
        jmethodID mid = env->GetMethodID(clazz, "findEncoderForSurfaceInput", "(Ljava/lang/String;)Ljava/lang/String;");
        if (mid) {
            jstring mimeStr = env->NewStringUTF(mime.c_str());
            jstring resultStr = static_cast<jstring>(env->CallObjectMethod(s->javaObj, mid, mimeStr));
            env->DeleteLocalRef(mimeStr);
            if (resultStr) {
                const char *chars = env->GetStringUTFChars(resultStr, nullptr);
                result = chars;
                env->ReleaseStringUTFChars(resultStr, chars);
                env->DeleteLocalRef(resultStr);
            }
        }
    }

    if (attached) s->jvm->DetachCurrentThread();
    return result;
}

void onCameraDeviceError(void *context, ACameraDevice *device, int error) {
    auto *s = static_cast<MediaSession *>(context);
    LOGE("[CRITICAL] CAMERA DEVICE ERROR: session=%p error=%d — closing camera", s, error);
    s->cameraError.store(error);
    notifyCameraError(s, error);
}

void onCameraDeviceDisconnected(void *context, ACameraDevice *device) {
    auto *s = static_cast<MediaSession *>(context);
    LOGE("[CRITICAL] CAMERA DEVICE DISCONNECTED: session=%p", s);
    s->cameraError.store(1);
    notifyCameraError(s, 1);
}