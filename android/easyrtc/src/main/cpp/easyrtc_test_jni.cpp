#include "easyrtc_video_decoder.h"
#include "easyrtc_frame_reader.h"
#include <jni.h>
#include <android/native_window_jni.h>
#include <thread>
#include <chrono>

extern "C" JNIEXPORT void JNICALL
Java_cn_easydarwin_easyrtc_DecoderPlaybackTest_nativeReplayFrames(
        JNIEnv* env, jclass clazz,
        jstring filePath, jobject surface, jint codec) {

    const char* path = env->GetStringUTFChars(filePath, nullptr);
    std::string pathStr(path);
    env->ReleaseStringUTFChars(filePath, path);

    FrameFileHeader header;
    std::vector<FrameRecord> frames;
    if (!frameReaderParse(pathStr, header, frames)) {
        return;
    }

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);

    VideoDecoderPipeline* decoder = videoDecoderCreate(window, codec, 720, 1280);
    if (!decoder) {
        ANativeWindow_release(window);
        return;
    }
    videoDecoderStart(decoder);

    for (size_t i = 0; i < frames.size(); i++) {
        auto& f = frames[i];
        if (f.kind != 0 || f.data.empty()) continue;

        if (i > 0) {
            int64_t deltaNs = f.callbackNs - frames[i - 1].callbackNs;
            if (deltaNs > 0 && deltaNs < 500000000LL) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(deltaNs));
            }
        }

        videoDecoderEnqueueFrame(decoder, f.data.data(), static_cast<int32_t>(f.data.size()), f.ptsUs, f.flags);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    videoDecoderRelease(decoder);
    ANativeWindow_release(window);
}
