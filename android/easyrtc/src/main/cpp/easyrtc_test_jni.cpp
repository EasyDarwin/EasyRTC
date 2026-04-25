#include "easyrtc_video_decoder.h"
#include "easyrtc_audio_playback.h"
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

    AudioPlaybackPipeline* audioPlayback = audioPlaybackCreate();

    int64_t lastPts = 0;
    for (size_t i = 0; i < frames.size(); i++) {
        auto& f = frames[i];
        auto ptsDeltaUS = f.ptsUs - lastPts;
        lastPts = f.ptsUs;
        if (i > 0) {
            int64_t deltaNs = f.callbackNs - frames[i - 1].callbackNs;
            LOGD("pts delta:%ld, callback delta:%ld", ptsDeltaUS, deltaNs/1000);
            if (deltaNs > 0 ) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(deltaNs));
            }
        }
        if (f.data.empty()) continue;
        if (f.kind == 0) {
            videoDecoderEnqueueFrame(decoder, f.data.data(), static_cast<int32_t>(f.data.size()), f.ptsUs, f.flags);
        } else if (f.kind == 1) {
            audioPlaybackEnqueueFrame(audioPlayback, f.data.data(), static_cast<int32_t>(f.data.size()));
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    audioPlaybackRelease(audioPlayback);
    videoDecoderRelease(decoder);
}
