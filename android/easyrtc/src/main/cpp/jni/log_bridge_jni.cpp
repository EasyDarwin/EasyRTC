#include "util/logger.h"
#include <jni.h>

extern "C" JNIEXPORT void JNICALL
Java_cn_easyrtc_NativeLogBridge_dispatch(JNIEnv* env, jclass, jstring message) {
    const char* text = env->GetStringUTFChars(message, nullptr);
    easyrtc_log_print(ANDROID_LOG_INFO, "EasyRTC.Java", "%s", text ? text : "");
    env->ReleaseStringUTFChars(message, text);
}
