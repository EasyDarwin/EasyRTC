# Android Native Media Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate audio capture, remote audio playback, and remote video decoding from Java/Kotlin to native C++ within `libeasyrtc_media.so`, and clean up the existing `dlsym` hack in favor of direct linking against `libEasyRTC.so` via `EasyRTCAPI.h`.

**Architecture:** Multi-.cpp files compiled into a single `libeasyrtc_media.so`. Each module (camera+encoder, audio capture, audio playback, video decoder) gets its own `.cpp`/`.h` pair. A shared `easyrtc_common.h` provides common logging macros and includes `EasyRTCAPI.h`. The existing `dlopen/dlsym` code in `easyrtc_media.cpp` is replaced with direct `#include "EasyRTCAPI.h"` and linking.

**Tech Stack:** C++17, NDK AAudio (API 28+), NDK MediaCodec, NDK Camera2, libEasyRTC.so (prebuilt)

---

## File Structure

```
easyrtc/src/main/cpp/
  easyrtc_common.h              # Shared: logging, EasyRTCAPI.h include
  easyrtc_media.h               # Existing: MediaPipeline struct (modified to use EasyRTCAPI.h types)
  easyrtc_media.cpp             # Existing: Camera+Encoder (modified: remove dlsym)
  easyrtc_audio.h               # NEW: AudioCapturePipeline struct
  easyrtc_audio.cpp             # NEW: AAudio recording + AEC + EasyRTC_SendFrame
  easyrtc_audio_playback.h      # NEW: AudioPlaybackPipeline struct
  easyrtc_audio_playback.cpp    # NEW: AAudio playback with jitter buffer
  easyrtc_video_decoder.h       # NEW: VideoDecoderPipeline struct
  easyrtc_video_decoder.cpp     # NEW: NDK MediaCodec decoder
  CMakeLists.txt                # Modified: add new sources, include path for EasyRTCAPI.h, link aaudio

easyrtc/src/main/java/cn/easyrtc/
  media/MediaPipeline.kt        # Existing (unchanged)
  media/AudioCapturePipeline.kt # NEW: Kotlin JNI bridge
  media/AudioPlaybackPipeline.kt # NEW: Kotlin JNI bridge
  media/VideoDecoderPipeline.kt  # NEW: Kotlin JNI bridge

app/src/main/java/.../fragment/
  HomeFragment.kt               # Modified: use new native pipelines instead of Java helpers
```

---

## Task 1: Create shared header and update CMakeLists.txt

**Files:**
- Create: `easyrtc/src/main/cpp/easyrtc_common.h`
- Modify: `easyrtc/src/main/cpp/CMakeLists.txt`

- [ ] **Step 1: Create `easyrtc_common.h`**

```cpp
#ifndef EASYRTC_COMMON_H
#define EASYRTC_COMMON_H

#include <android/log.h>

#define LOG_TAG "EasyRTCMedia"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

#define COLOR_FormatSurface 0x7f420888
#define BUFFER_FLAG_CODEC_CONFIG 2

#endif
```

- [ ] **Step 2: Update `CMakeLists.txt`**

Replace the entire file with:

```cmake
cmake_minimum_required(VERSION 3.22)
project(easyrtc_media)

set(REPO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include")

add_library(easyrtc_media SHARED
    easyrtc_media.cpp
    easyrtc_audio.cpp
    easyrtc_audio_playback.cpp
    easyrtc_video_decoder.cpp
)

target_include_directories(easyrtc_media PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REPO_INCLUDE_DIR}
)

target_link_libraries(easyrtc_media
    android
    log
    mediandk
    camera2ndk
    aaudio
    ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libEasyRTC.so
)

set_target_properties(easyrtc_media PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
```

Key changes: added `easyrtc_audio.cpp`, `easyrtc_audio_playback.cpp`, `easyrtc_video_decoder.cpp` to sources; added `aaudio` link; added `REPO_INCLUDE_DIR` for `EasyRTCAPI.h`.

- [ ] **Step 3: Commit**

```bash
git add easyrtc/src/main/cpp/easyrtc_common.h easyrtc/src/main/cpp/CMakeLists.txt
git commit -m "build: add shared header and update CMakeLists for native media migration"
```

---

## Task 2: Clean up existing easyrtc_media.cpp - remove dlsym

**Files:**
- Modify: `easyrtc/src/main/cpp/easyrtc_media.h`
- Modify: `easyrtc/src/main/cpp/easyrtc_media.cpp`

- [ ] **Step 1: Update `easyrtc_media.h`**

Replace the entire file with:

```cpp
#ifndef EASYRTC_MEDIA_H
#define EASYRTC_MEDIA_H

#include "easyrtc_common.h"
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadataTags.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCaptureRequest.h>
#include <android/native_window_jni.h>
#include <pthread.h>
#include <atomic>
#include <cstdint>

struct MediaPipeline {
    AMediaCodec* encoder = nullptr;
    AMediaFormat* format = nullptr;
    ANativeWindow* window = nullptr;
    void* transceiver = nullptr;
    int width = 0;
    int height = 0;
    int bitrate = 0;
    int fps = 0;
    int iframeInterval = 0;
    std::atomic<bool> running{false};
    pthread_t output_thread = 0;
    uint8_t* sps_pps_buffer = nullptr;
    size_t sps_pps_size = 0;
    pthread_mutex_t mutex;

    ACameraDevice* cameraDevice = nullptr;
    ACameraCaptureSession* captureSession = nullptr;
    ACaptureSessionOutputContainer* outputContainer = nullptr;
    ACaptureSessionOutput* encoderOutput = nullptr;
    ACaptureSessionOutput* previewOutput = nullptr;
    ACameraOutputTarget* encoderTarget = nullptr;
    ACameraOutputTarget* previewTarget = nullptr;
    ACaptureRequest* captureRequest = nullptr;
    ANativeWindow* previewWindow = nullptr;
    int cameraFacing = 0;

    MediaPipeline() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    MediaPipeline(const MediaPipeline&) = delete;
    MediaPipeline& operator=(const MediaPipeline&) = delete;
    MediaPipeline(MediaPipeline&&) = delete;
    MediaPipeline& operator=(MediaPipeline&&) = delete;
};

#endif
```

The only change from the original is replacing the first block of includes (media/NdkMediaCodec.h etc.) with `#include "easyrtc_common.h"` first, keeping the rest identical.

- [ ] **Step 2: Update `easyrtc_media.cpp`**

Make the following changes to the existing file:

1. Replace the includes block at the top (lines 1-7) with:
```cpp
#include "easyrtc_media.h"
#include <jni.h>
#include <cstring>
#include <string>
```
(Removes `<dlfcn.h>`, removes redundant `<android/log.h>` — now via `easyrtc_common.h`)

2. Remove the LOG_TAG and LOG macros (lines 8-10) — now in `easyrtc_common.h`

3. Remove `#define COLOR_FormatSurface 0x7f420888` (line 12) — now in `easyrtc_common.h`

4. Remove `#define BUFFER_FLAG_CODEC_CONFIG 2` (line 14) — now in `easyrtc_common.h`

5. Remove the local typedef block (lines 16-17):
```cpp
// DELETE these lines:
typedef unsigned int UINT32;
typedef unsigned long long UINT64;
```
These are provided by `EasyRTCAPI.h` (included via `easyrtc_common.h`) when `ANDROID_BUILD` is defined. Note: `EasyRTCAPI.h` defines `UINT32`, `UINT64`, `PBYTE`, `BOOL` etc. under `#ifdef ANDROID_BUILD`. We need to ensure `ANDROID_BUILD` is defined. Add to CMakeLists.txt (see step below).

6. Remove the local `EasyRTC_Frame` struct definition (lines 19-29):
```cpp
// DELETE this entire block:
typedef struct __EasyRTC_Frame {
    ...
} EasyRTC_Frame;
```
This is now provided by `EasyRTCAPI.h`.

7. Remove the `dlsym` function pointer and loader (lines 31-61):
```cpp
// DELETE all of this:
typedef int (*EasyRTC_SendFrame_t)(void*, void*);
static EasyRTC_SendFrame_t g_sendFrame = nullptr;
static void* g_dlHandle = nullptr;
static int sendFrame(void* transceiver, void* frame) { ... }
__attribute__((constructor)) static void loadSendFrame() { ... }
__attribute__((destructor)) static void unloadSendFrame() { ... }
```

8. Replace the call in `outputThreadFunc` (line 374):
Change: `int sendResult = sendFrame(realTransceiver, &frame);`
To: `int sendResult = EasyRTC_SendFrame(realTransceiver, &frame);`

9. Change: `EasyRTC_Frame frame{};` (line 336) — this should now work directly since `EasyRTCAPI.h` defines `EasyRTC_Frame` (note: the API header uses `__EasyRTC_Frame` as the struct tag and `EasyRTC_Frame` as the typedef, same as what was locally defined).

- [ ] **Step 3: Add `ANDROID_BUILD` define to CMakeLists.txt**

Add this line before `set_target_properties`:
```cmake
target_compile_definitions(easyrtc_media PRIVATE ANDROID_BUILD)
```

This is required because `EasyRTCAPI.h` only defines `UINT32`, `PBYTE`, `BOOL` etc. under `#ifdef ANDROID_BUILD`.

- [ ] **Step 4: Build and verify**

Run: `./gradlew :easyrtc:assembleRelease`
Expected: Build succeeds with no errors (warnings from existing code are OK).

- [ ] **Step 5: Commit**

```bash
git add easyrtc/src/main/cpp/easyrtc_media.h easyrtc/src/main/cpp/easyrtc_media.cpp easyrtc/src/main/cpp/CMakeLists.txt
git commit -m "refactor: replace dlsym with direct linking to libEasyRTC.so via EasyRTCAPI.h"
```

---

## Task 3: Implement native audio capture pipeline

**Files:**
- Create: `easyrtc/src/main/cpp/easyrtc_audio.h`
- Create: `easyrtc/src/main/cpp/easyrtc_audio.cpp`
- Create: `easyrtc/src/main/java/cn/easyrtc/media/AudioCapturePipeline.kt`

- [ ] **Step 1: Create `easyrtc_audio.h`**

```cpp
#ifndef EASYRTC_AUDIO_H
#define EASYRTC_AUDIO_H

#include "easyrtc_common.h"
#include <aaudio/AAudio.h>
#include <pthread.h>
#include <atomic>
#include <cstdint>

struct AudioCapturePipeline {
    AAudioStream* stream = nullptr;
    void* audioTransceiver = nullptr;
    std::atomic<bool> running{false};
    pthread_mutex_t mutex;

    AudioCapturePipeline() {
        pthread_mutex_init(&mutex, nullptr);
    }
    ~AudioCapturePipeline() {
        pthread_mutex_destroy(&mutex);
    }
    AudioCapturePipeline(const AudioCapturePipeline&) = delete;
    AudioCapturePipeline& operator=(const AudioCapturePipeline&) = delete;
};

#endif
```

- [ ] **Step 2: Create `easyrtc_audio.cpp`**

```cpp
#include "easyrtc_audio.h"
#include <jni.h>
#include <cstring>

static constexpr int32_t SAMPLE_RATE = 8000;
static constexpr int32_t CHANNEL_COUNT = 1;
static constexpr int32_t FRAME_SIZE = 320;

static oboe::DataCallbackResult onDataCallback(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames) {
    auto* pipeline = static_cast<AudioCapturePipeline*>(userData);
    if (!pipeline || !pipeline->running.load()) {
        return oboe::DataCallbackResult::Stop;
    }

    if (!pipeline->audioTransceiver) {
        return oboe::DataCallbackResult::Continue;
    }

    void* realTransceiver = nullptr;
    auto* transceiverPtr = reinterpret_cast<void**>(pipeline->audioTransceiver);
    if (transceiverPtr) {
        realTransceiver = *transceiverPtr;
    }
    if (!realTransceiver) {
        return oboe::DataCallbackResult::Continue;
    }

    int32_t dataSize = numFrames * CHANNEL_COUNT * sizeof(int16_t);
    auto* pcmData = static_cast<uint8_t*>(audioData);

    EasyRTC_Frame frame{};
    frame.version = 0;
    frame.size = static_cast<UINT32>(dataSize);
    frame.flags = EASYRTC_FRAME_FLAG_NONE;
    frame.presentationTs = static_cast<UINT64>(
            AAudioStream_getTimestamp(stream, CLOCK_MONOTONIC) / 100);
    frame.decodingTs = frame.presentationTs;
    frame.frameData = pcmData;
    frame.trackId = 0;

    int result = EasyRTC_SendFrame(realTransceiver, &frame);
    if (result != 0) {
        LOGE("EasyRTC_SendFrame (audio) failed: %d, size=%u", result, frame.size);
    }

    return oboe::DataCallbackResult::Continue;
}
```

Wait — we should NOT use oboe. We're using AAudio directly. Let me correct:

```cpp
#include "easyrtc_audio.h"
#include <jni.h>
#include <cstring>
#include <ctime>

static constexpr int32_t SAMPLE_RATE = 8000;
static constexpr int32_t CHANNEL_COUNT = 1;

static aaudio_data_callback_result_t dataCallback(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames) {
    auto* pipeline = static_cast<AudioCapturePipeline*>(userData);
    if (!pipeline || !pipeline->running.load()) {
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    if (!pipeline->audioTransceiver) {
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    void* realTransceiver = nullptr;
    auto* transceiverPtr = reinterpret_cast<void**>(pipeline->audioTransceiver);
    if (transceiverPtr) {
        realTransceiver = *transceiverPtr;
    }
    if (!realTransceiver) {
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    int32_t dataSize = numFrames * CHANNEL_COUNT * sizeof(int16_t);
    auto* pcmData = static_cast<PBYTE>(audioData);

    int64_t frameIndex = 0;
    int64_t timeNanos = 0;
    AAudioStream_getTimestamp(stream, CLOCK_MONOTONIC, &frameIndex, &timeNanos);

    EasyRTC_Frame frame{};
    frame.version = 0;
    frame.size = static_cast<UINT32>(dataSize);
    frame.flags = static_cast<EasyRTC_FRAME_FLAGS>(EASYRTC_FRAME_FLAG_NONE);
    frame.presentationTs = static_cast<UINT64>(timeNanos / 100);
    frame.decodingTs = frame.presentationTs;
    frame.frameData = pcmData;
    frame.trackId = 0;

    int result = EasyRTC_SendFrame(realTransceiver, &frame);
    if (result != 0) {
        LOGE("EasyRTC_SendFrame (audio) failed: %d", result);
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void errorCallback(
        AAudioStream* stream,
        void* userData,
        aaudio_result_t error) {
    LOGE("AAudio capture error: %d", error);
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_AudioCapturePipeline_nativeCreate(
        JNIEnv* env, jobject thiz, jlong transceiverHandle) {
    auto* pipeline = new AudioCapturePipeline();
    pipeline->audioTransceiver = reinterpret_cast<void*>(transceiverHandle);
    LOGD("AudioCapturePipeline created, transceiver=%p", pipeline->audioTransceiver);
    return reinterpret_cast<jlong>(pipeline);
}

JNIEXPORT jint JNICALL
Java_cn_easyrtc_media_AudioCapturePipeline_nativeStart(
        JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<AudioCapturePipeline*>(pipelinePtr);
    if (!pipeline) return -1;

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || !builder) {
        LOGE("Failed to create AAudio stream builder: %d", result);
        return -1;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
    AAudioStreamBuilder_setSampleRate(builder, SAMPLE_RATE);
    AAudioStreamBuilder_setChannelCount(builder, CHANNEL_COUNT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
    AAudioStreamBuilder_setDataCallback(builder, dataCallback, pipeline);
    AAudioStreamBuilder_setErrorCallback(builder, errorCallback, pipeline);

    AAudioStream* stream = nullptr;
    result = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || !stream) {
        LOGE("Failed to open AAudio input stream: %d", result);
        return -1;
    }

    pipeline->stream = stream;
    pipeline->running.store(true);

    result = AAudioStream_requestStart(stream);
    if (result != AAUDIO_OK) {
        LOGE("Failed to start AAudio input stream: %d", result);
        AAudioStream_close(stream);
        pipeline->stream = nullptr;
        pipeline->running.store(false);
        return -1;
    }

    LOGD("Audio capture started: %dHz, %dch", SAMPLE_RATE, CHANNEL_COUNT);
    return 0;
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_AudioCapturePipeline_nativeStop(
        JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<AudioCapturePipeline*>(pipelinePtr);
    if (!pipeline) return;

    pipeline->running.store(false);

    if (pipeline->stream) {
        AAudioStream_requestStop(pipeline->stream);
        AAudioStream_close(pipeline->stream);
        pipeline->stream = nullptr;
    }

    LOGD("Audio capture stopped");
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_AudioCapturePipeline_nativeRelease(
        JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<AudioCapturePipeline*>(pipelinePtr);
    if (!pipeline) return;

    Java_cn_easyrtc_media_AudioCapturePipeline_nativeStop(env, thiz, pipelinePtr);
    delete pipeline;
    LOGD("AudioCapturePipeline released");
}

}
```

- [ ] **Step 3: Create `AudioCapturePipeline.kt`**

```kotlin
package cn.easyrtc.media

class AudioCapturePipeline(private val transceiverHandle: Long) {

    private var nativePtr: Long = nativeCreate(transceiverHandle)

    fun start(): Int {
        return nativeStart(nativePtr)
    }

    fun stop() {
        nativeStop(nativePtr)
    }

    fun release() {
        if (nativePtr != 0L) {
            nativeRelease(nativePtr)
            nativePtr = 0L
        }
    }

    private external fun nativeCreate(transceiverHandle: Long): Long
    private external fun nativeStart(pipelinePtr: Long): Int
    private external fun nativeStop(pipelinePtr: Long)
    private external fun nativeRelease(pipelinePtr: Long)

    companion object {
        init {
            System.loadLibrary("easyrtc_media")
        }
    }
}
```

- [ ] **Step 4: Build and verify**

Run: `./gradlew :easyrtc:assembleRelease`
Expected: Build succeeds.

- [ ] **Step 5: Integrate into HomeFragment**

In `HomeFragment.kt`:

1. Add import: `import cn.easyrtc.media.AudioCapturePipeline`
2. Remove import: `import cn.easyrtc.helper.AudioHelper`
3. Change class declaration — remove `AudioHelper.OnAudioDataListener`:
   Change: `class HomeFragment : Fragment(), TextureView.SurfaceTextureListener, AudioHelper.OnAudioDataListener, WebSocketManager.WebSocketCallback,`
   To: `class HomeFragment : Fragment(), TextureView.SurfaceTextureListener, WebSocketManager.WebSocketCallback,`
4. Replace field declaration:
   Change: `private var audioHelper: AudioHelper? = null`
   To: `private var audioCapturePipeline: AudioCapturePipeline? = null`
5. In `connectionStateChange()`, replace the audio start block (lines 437-441):
   Change:
   ```kotlin
   val audioTransceiver = EasyRTCSdk.getAudioTransceiver()
   if (audioTransceiver != 0L && audioHelper == null) {
       audioHelper = AudioHelper()
       audioHelper?.setOnAudioDataListener(this)
       audioHelper?.start()
   }
   ```
   To:
   ```kotlin
   val audioTransceiver = EasyRTCSdk.getAudioTransceiver()
   if (audioTransceiver != 0L && audioCapturePipeline == null) {
       audioCapturePipeline = AudioCapturePipeline(audioTransceiver)
       audioCapturePipeline?.start()
   }
   ```
6. In `stopEasyRTC()`, replace audio cleanup (lines 550, 553):
   Change: `audioHelper?.stop()` and `audioHelper = null`
   To: `audioCapturePipeline?.release()` and `audioCapturePipeline = null`
7. Remove the `onAudioData` and `onError` override methods (lines 379-384):
   ```kotlin
   // DELETE these:
   override fun onAudioData(data: ByteArray, pts: Long) { ... }
   override fun onError(error: String) { }
   ```

- [ ] **Step 6: Build and test on device**

Run: `./gradlew :app:assembleDebug`
Deploy to device/emulator and verify audio capture still works in a call.

- [ ] **Step 7: Commit**

```bash
git add easyrtc/src/main/cpp/easyrtc_audio.h easyrtc/src/main/cpp/easyrtc_audio.cpp easyrtc/src/main/java/cn/easyrtc/media/AudioCapturePipeline.kt app/src/main/java/cn/easydarwin/easyrtc/fragment/HomeFragment.kt
git commit -m "feat: migrate audio capture to native AAudio pipeline"
```

---

## Task 4: Implement native audio playback pipeline

**Files:**
- Create: `easyrtc/src/main/cpp/easyrtc_audio_playback.h`
- Create: `easyrtc/src/main/cpp/easyrtc_audio_playback.cpp`
- Create: `easyrtc/src/main/java/cn/easyrtc/media/AudioPlaybackPipeline.kt`

- [ ] **Step 1: Create `easyrtc_audio_playback.h`**

```cpp
#ifndef EASYRTC_AUDIO_PLAYBACK_H
#define EASYRTC_AUDIO_PLAYBACK_H

#include "easyrtc_common.h"
#include <aaudio/AAudio.h>
#include <pthread.h>
#include <atomic>
#include <cstdint>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>

struct AudioPlaybackPipeline {
    AAudioStream* stream = nullptr;
    std::queue<std::vector<uint8_t>> jitterBuffer;
    std::mutex queueMutex;
    std::condition_variable queueCv;
    std::atomic<bool> playing{false};
    std::atomic<bool> stopped{false};

    static constexpr size_t MAX_QUEUE_SIZE = 200;
    static constexpr int32_t SAMPLE_RATE = 8000;
    static constexpr int32_t CHANNEL_COUNT = 1;
    static constexpr size_t FRAME_SIZE = 320;

    AudioPlaybackPipeline() = default;
    ~AudioPlaybackPipeline() = default;
    AudioPlaybackPipeline(const AudioPlaybackPipeline&) = delete;
    AudioPlaybackPipeline& operator=(const AudioPlaybackPipeline&) = delete;
};

#endif
```

- [ ] **Step 2: Create `easyrtc_audio_playback.cpp`**

```cpp
#include "easyrtc_audio_playback.h"
#include <jni.h>
#include <cstring>
#include <chrono>

static aaudio_data_callback_result_t playbackCallback(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames) {
    auto* pipeline = static_cast<AudioPlaybackPipeline*>(userData);
    if (!pipeline || pipeline->stopped.load()) {
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    int32_t requestedBytes = numFrames * pipeline->CHANNEL_COUNT * sizeof(int16_t);
    auto* output = static_cast<uint8_t*>(audioData);

    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        if (!pipeline->jitterBuffer.empty()) {
            frame = std::move(pipeline->jitterBuffer.front());
            pipeline->jitterBuffer.pop();
        }
    }

    if (!frame.empty()) {
        int32_t toCopy = std::min(static_cast<int32_t>(frame.size()), requestedBytes);
        memcpy(output, frame.data(), toCopy);
        if (toCopy < requestedBytes) {
            memset(output + toCopy, 0, requestedBytes - toCopy);
        }
    } else {
        memset(output, 0, requestedBytes);
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void playbackErrorCallback(
        AAudioStream* stream,
        void* userData,
        aaudio_result_t error) {
    LOGE("AAudio playback error: %d", error);
}

static int ensureStreamCreated(AudioPlaybackPipeline* pipeline) {
    if (pipeline->stream) return 0;

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || !builder) {
        LOGE("Failed to create AAudio stream builder: %d", result);
        return -1;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(builder, pipeline->SAMPLE_RATE);
    AAudioStreamBuilder_setChannelCount(builder, pipeline->CHANNEL_COUNT);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_VOICE_COMMUNICATION);
    AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_SPEECH);
    AAudioStreamBuilder_setDataCallback(builder, playbackCallback, pipeline);
    AAudioStreamBuilder_setErrorCallback(builder, playbackErrorCallback, pipeline);

    AAudioStream* stream = nullptr;
    result = AAudioStreamBuilder_openStream(builder, &stream);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || !stream) {
        LOGE("Failed to open AAudio output stream: %d", result);
        return -1;
    }

    pipeline->stream = stream;
    pipeline->stopped.store(false);

    result = AAudioStream_requestStart(stream);
    if (result != AAUDIO_OK) {
        LOGE("Failed to start AAudio output stream: %d", result);
        AAudioStream_close(stream);
        pipeline->stream = nullptr;
        return -1;
    }

    pipeline->playing.store(true);
    LOGD("Audio playback started: %dHz, %dch", pipeline->SAMPLE_RATE, pipeline->CHANNEL_COUNT);
    return 0;
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_AudioPlaybackPipeline_nativeCreate(
        JNIEnv* env, jobject thiz) {
    auto* pipeline = new AudioPlaybackPipeline();
    LOGD("AudioPlaybackPipeline created");
    return reinterpret_cast<jlong>(pipeline);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_AudioPlaybackPipeline_nativeWriteFrame(
        JNIEnv* env, jobject thiz, jlong pipelinePtr, jbyteArray data, jint size) {
    auto* pipeline = reinterpret_cast<AudioPlaybackPipeline*>(pipelinePtr);
    if (!pipeline || pipeline->stopped.load() || size <= 0) return;

    if (!pipeline->playing.load()) {
        ensureStreamCreated(pipeline);
    }

    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    if (!bytes) return;

    std::vector<uint8_t> frame(bytes, bytes + size);
    env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);

    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        if (pipeline->jitterBuffer.size() >= pipeline->MAX_QUEUE_SIZE) {
            pipeline->jitterBuffer.pop();
            LOGW("Audio playback queue overflow, dropped oldest frame");
        }
        pipeline->jitterBuffer.push(std::move(frame));
    }
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_AudioPlaybackPipeline_nativeRelease(
        JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<AudioPlaybackPipeline*>(pipelinePtr);
    if (!pipeline) return;

    pipeline->stopped.store(true);
    pipeline->playing.store(false);

    if (pipeline->stream) {
        AAudioStream_requestStop(pipeline->stream);
        AAudioStream_close(pipeline->stream);
        pipeline->stream = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        while (!pipeline->jitterBuffer.empty()) {
            pipeline->jitterBuffer.pop();
        }
    }

    delete pipeline;
    LOGD("AudioPlaybackPipeline released");
}

}
```

- [ ] **Step 3: Create `AudioPlaybackPipeline.kt`**

```kotlin
package cn.easyrtc.media

class AudioPlaybackPipeline {

    private var nativePtr: Long = nativeCreate()

    fun writeFrame(data: ByteArray, size: Int) {
        if (nativePtr != 0L) {
            nativeWriteFrame(nativePtr, data, size)
        }
    }

    fun release() {
        if (nativePtr != 0L) {
            nativeRelease(nativePtr)
            nativePtr = 0L
        }
    }

    private external fun nativeCreate(): Long
    private external fun nativeWriteFrame(pipelinePtr: Long, data: ByteArray, size: Int)
    private external fun nativeRelease(pipelinePtr: Long)

    companion object {
        init {
            System.loadLibrary("easyrtc_media")
        }
    }
}
```

- [ ] **Step 4: Build and verify**

Run: `./gradlew :easyrtc:assembleRelease`
Expected: Build succeeds.

- [ ] **Step 5: Integrate into HomeFragment**

In `HomeFragment.kt`:

1. Add import: `import cn.easyrtc.media.AudioPlaybackPipeline`
2. Remove import: `import cn.easyrtc.helper.RemoteRTCAudioHelper`
3. Replace field: `private var mRemoteRTCAudioHelper: RemoteRTCAudioHelper? = null` with `private var audioPlaybackPipeline: AudioPlaybackPipeline? = null`
4. In `connectionStateChange()`, replace the audio playback init (lines 444-446):
   Change:
   ```kotlin
   if (mRemoteRTCAudioHelper == null) {
       mRemoteRTCAudioHelper = RemoteRTCAudioHelper(requireContext())
   }
   ```
   To:
   ```kotlin
   if (audioPlaybackPipeline == null) {
       audioPlaybackPipeline = AudioPlaybackPipeline()
   }
   ```
5. In `onTransceiverCallback()`, replace the audio path (line 467):
   Change: `mRemoteRTCAudioHelper?.processRemoteAudioFrameSafe(frameData, frameData.size)`
   To: `audioPlaybackPipeline?.writeFrame(frameData, frameData.size)`
6. In `stopEasyRTC()`, replace cleanup (lines 552, 555):
   Change: `mRemoteRTCAudioHelper?.release()` and `mRemoteRTCAudioHelper = null`
   To: `audioPlaybackPipeline?.release()` and `audioPlaybackPipeline = null`

- [ ] **Step 6: Build and test on device**

Run: `./gradlew :app:assembleDebug`
Deploy and verify remote audio playback still works.

- [ ] **Step 7: Commit**

```bash
git add easyrtc/src/main/cpp/easyrtc_audio_playback.h easyrtc/src/main/cpp/easyrtc_audio_playback.cpp easyrtc/src/main/java/cn/easyrtc/media/AudioPlaybackPipeline.kt app/src/main/java/cn/easydarwin/easyrtc/fragment/HomeFragment.kt
git commit -m "feat: migrate remote audio playback to native AAudio pipeline"
```

---

## Task 5: Implement native video decoder pipeline

**Files:**
- Create: `easyrtc/src/main/cpp/easyrtc_video_decoder.h`
- Create: `easyrtc/src/main/cpp/easyrtc_video_decoder.cpp`
- Create: `easyrtc/src/main/java/cn/easyrtc/media/VideoDecoderPipeline.kt`

- [ ] **Step 1: Create `easyrtc_video_decoder.h`**

```cpp
#ifndef EASYRTC_VIDEO_DECODER_H
#define EASYRTC_VIDEO_DECODER_H

#include "easyrtc_common.h"
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <android/native_window_jni.h>
#include <pthread.h>
#include <atomic>
#include <cstdint>
#include <queue>
#include <vector>
#include <mutex>
#include <string>

struct VideoDecoderPipeline {
    AMediaCodec* decoder = nullptr;
    ANativeWindow* surface = nullptr;
    std::queue<std::vector<uint8_t>> frameQueue;
    std::mutex queueMutex;
    std::atomic<bool> running{false};
    std::atomic<bool> destroyed{false};
    pthread_t decodeThread = 0;
    std::string currentCodecType;
    int width = 0;
    int height = 0;
    int frameRate = 30;
    std::atomic<int> errorCount{0};
    pthread_mutex_t mutex;

    static constexpr int MAX_ERROR_COUNT = 5;
    static constexpr size_t MAX_QUEUE_SIZE = 30;
    static constexpr int64_t DEQUEUE_TIMEOUT_US = 10000;

    VideoDecoderPipeline() {
        pthread_mutex_init(&mutex, nullptr);
    }
    ~VideoDecoderPipeline() {
        pthread_mutex_destroy(&mutex);
    }
    VideoDecoderPipeline(const VideoDecoderPipeline&) = delete;
    VideoDecoderPipeline& operator=(const VideoDecoderPipeline&) = delete;
};

#endif
```

- [ ] **Step 2: Create `easyrtc_video_decoder.cpp`**

```cpp
#include "easyrtc_video_decoder.h"
#include <jni.h>
#include <cstring>
#include <unistd.h>

static bool initDecoder(VideoDecoderPipeline* pipeline) {
    pthread_mutex_lock(&pipeline->mutex);

    if (pipeline->decoder) {
        AMediaCodec_stop(pipeline->decoder);
        AMediaCodec_delete(pipeline->decoder);
        pipeline->decoder = nullptr;
    }

    pipeline->decoder = AMediaCodec_createDecoderByType(pipeline->currentCodecType.c_str());
    if (!pipeline->decoder) {
        LOGE("Failed to create video decoder for %s", pipeline->currentCodecType.c_str());
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, pipeline->currentCodecType.c_str());
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, pipeline->width);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, pipeline->height);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, pipeline->frameRate);

    media_status_t status = AMediaCodec_configure(pipeline->decoder, format,
            pipeline->surface, nullptr, 0);
    AMediaFormat_delete(format);

    if (status != AMEDIA_OK) {
        LOGE("Failed to configure video decoder: %d", status);
        AMediaCodec_delete(pipeline->decoder);
        pipeline->decoder = nullptr;
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    status = AMediaCodec_start(pipeline->decoder);
    if (status != AMEDIA_OK) {
        LOGE("Failed to start video decoder: %d", status);
        AMediaCodec_delete(pipeline->decoder);
        pipeline->decoder = nullptr;
        pthread_mutex_unlock(&pipeline->mutex);
        return false;
    }

    pipeline->errorCount.store(0);
    LOGD("Video decoder initialized: %s %dx%d", pipeline->currentCodecType.c_str(),
            pipeline->width, pipeline->height);

    pthread_mutex_unlock(&pipeline->mutex);
    return true;
}

static void* decodeThreadFunc(void* arg) {
    auto* pipeline = static_cast<VideoDecoderPipeline*>(arg);
    LOGD("Video decode thread started");
    AMediaCodecBufferInfo bufferInfo;

    while (pipeline->running.load() && !pipeline->destroyed.load()) {
        std::vector<uint8_t> frameData;
        {
            std::lock_guard<std::mutex> lock(pipeline->queueMutex);
            if (!pipeline->frameQueue.empty()) {
                frameData = std::move(pipeline->frameQueue.front());
                pipeline->frameQueue.pop();
            }
        }

        if (frameData.empty()) {
            usleep(1000);
            continue;
        }

        AMediaCodec* decoder = pipeline->decoder;
        if (!decoder) {
            usleep(5000);
            continue;
        }

        ssize_t inputBufId = AMediaCodec_dequeueInputBuffer(decoder, pipeline->DEQUEUE_TIMEOUT_US);
        if (inputBufId < 0) {
            continue;
        }

        size_t outSize = 0;
        uint8_t* inputBuf = AMediaCodec_getInputBuffer(decoder, inputBufId, &outSize);
        if (!inputBuf || frameData.size() > outSize) {
            AMediaCodec_releaseOutputBuffer(decoder, static_cast<size_t>(inputBufId), false);
            continue;
        }

        memcpy(inputBuf, frameData.data(), frameData.size());
        int64_t pts = static_cast<int64_t>(frameData.size());
        AMediaCodec_queueInputBuffer(decoder, inputBufId, 0, frameData.size(),
                static_cast<int64_t>(/* use system time */ 0), 0);

        while (pipeline->running.load() && !pipeline->destroyed.load()) {
            ssize_t outputBufId = AMediaCodec_dequeueOutputBuffer(decoder, &bufferInfo,
                    pipeline->DEQUEUE_TIMEOUT_US);
            if (outputBufId >= 0) {
                AMediaCodec_releaseOutputBuffer(decoder, outputBufId, true);
                pipeline->errorCount.store(0);
                break;
            } else if (outputBufId == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                break;
            } else if (outputBufId == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
                LOGD("Decoder output format changed");
            }
        }

        int errors = pipeline->errorCount.load();
        if (errors > 0 && errors % pipeline->MAX_ERROR_COUNT == 0) {
            LOGW("Too many decoder errors (%d), reinitializing", errors);
            initDecoder(pipeline);
        }
    }

    LOGD("Video decode thread exiting");
    return nullptr;
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_cn_easyrtc_media_VideoDecoderPipeline_nativeCreate(
        JNIEnv* env, jobject thiz, jobject surface, jint codecType, jint width, jint height) {
    auto* pipeline = new VideoDecoderPipeline();
    pipeline->width = width;
    pipeline->height = height;
    pipeline->currentCodecType = (codecType == 6) ? "video/hevc" : "video/avc";

    if (surface) {
        pipeline->surface = ANativeWindow_fromSurface(env, surface);
    }

    if (!initDecoder(pipeline)) {
        if (pipeline->surface) ANativeWindow_release(pipeline->surface);
        delete pipeline;
        return 0;
    }

    LOGD("VideoDecoderPipeline created: %s %dx%d", pipeline->currentCodecType.c_str(), width, height);
    return reinterpret_cast<jlong>(pipeline);
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_VideoDecoderPipeline_nativeStart(
        JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<VideoDecoderPipeline*>(pipelinePtr);
    if (!pipeline) return;

    pipeline->running.store(true);
    int ret = pthread_create(&pipeline->decodeThread, nullptr, decodeThreadFunc, pipeline);
    if (ret != 0) {
        LOGE("Failed to create decode thread: %d", ret);
        pipeline->running.store(false);
    }
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_VideoDecoderPipeline_nativeWriteFrame(
        JNIEnv* env, jobject thiz, jlong pipelinePtr, jbyteArray data, jint size) {
    auto* pipeline = reinterpret_cast<VideoDecoderPipeline*>(pipelinePtr);
    if (!pipeline || !pipeline->running.load() || pipeline->destroyed.load() || size <= 0) return;

    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    if (!bytes) return;

    std::vector<uint8_t> frame(bytes, bytes + size);
    env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);

    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        if (pipeline->frameQueue.size() >= pipeline->MAX_QUEUE_SIZE) {
            pipeline->frameQueue.pop();
        }
        pipeline->frameQueue.push(std::move(frame));
    }
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_VideoDecoderPipeline_nativeReinit(
        JNIEnv* env, jobject thiz, jlong pipelinePtr, jint codecType) {
    auto* pipeline = reinterpret_cast<VideoDecoderPipeline*>(pipelinePtr);
    if (!pipeline) return;

    std::string newCodec = (codecType == 6) ? "video/hevc" : "video/avc";
    if (newCodec == pipeline->currentCodecType) return;

    LOGD("Reinitializing video decoder: %s -> %s", pipeline->currentCodecType.c_str(), newCodec.c_str());

    pipeline->running.store(false);
    if (pipeline->decodeThread) {
        pthread_join(pipeline->decodeThread, nullptr);
        pipeline->decodeThread = 0;
    }

    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        while (!pipeline->frameQueue.empty()) pipeline->frameQueue.pop();
    }

    pipeline->currentCodecType = newCodec;
    initDecoder(pipeline);

    pipeline->running.store(true);
    int ret = pthread_create(&pipeline->decodeThread, nullptr, decodeThreadFunc, pipeline);
    if (ret != 0) {
        LOGE("Failed to restart decode thread: %d", ret);
    }
}

JNIEXPORT void JNICALL
Java_cn_easyrtc_media_VideoDecoderPipeline_nativeRelease(
        JNIEnv* env, jobject thiz, jlong pipelinePtr) {
    auto* pipeline = reinterpret_cast<VideoDecoderPipeline*>(pipelinePtr);
    if (!pipeline) return;

    pipeline->running.store(false);
    pipeline->destroyed.store(true);

    if (pipeline->decodeThread) {
        pthread_join(pipeline->decodeThread, nullptr);
        pipeline->decodeThread = 0;
    }

    {
        std::lock_guard<std::mutex> lock(pipeline->queueMutex);
        while (!pipeline->frameQueue.empty()) pipeline->frameQueue.pop();
    }

    pthread_mutex_lock(&pipeline->mutex);
    if (pipeline->decoder) {
        AMediaCodec_stop(pipeline->decoder);
        AMediaCodec_delete(pipeline->decoder);
        pipeline->decoder = nullptr;
    }
    pthread_mutex_unlock(&pipeline->mutex);

    if (pipeline->surface) {
        ANativeWindow_release(pipeline->surface);
        pipeline->surface = nullptr;
    }

    delete pipeline;
    LOGD("VideoDecoderPipeline released");
}

}
```

- [ ] **Step 3: Create `VideoDecoderPipeline.kt`**

```kotlin
package cn.easyrtc.media

import android.view.Surface

class VideoDecoderPipeline(
    surface: Surface,
    codecType: Int = CODEC_H264,
    width: Int = 720,
    height: Int = 1280
) {

    private var nativePtr: Long = nativeCreate(surface, codecType, width, height)

    init {
        if (nativePtr != 0L) {
            nativeStart(nativePtr)
        }
    }

    fun writeFrame(data: ByteArray, size: Int) {
        if (nativePtr != 0L) {
            nativeWriteFrame(nativePtr, data, size)
        }
    }

    fun reinit(codecType: Int) {
        if (nativePtr != 0L) {
            nativeReinit(nativePtr, codecType)
        }
    }

    fun release() {
        if (nativePtr != 0L) {
            nativeRelease(nativePtr)
            nativePtr = 0L
        }
    }

    private external fun nativeCreate(surface: Surface, codecType: Int, width: Int, height: Int): Long
    private external fun nativeStart(pipelinePtr: Long)
    private external fun nativeWriteFrame(pipelinePtr: Long, data: ByteArray, size: Int)
    private external fun nativeReinit(pipelinePtr: Long, codecType: Int)
    private external fun nativeRelease(pipelinePtr: Long)

    companion object {
        const val CODEC_H264 = 1
        const val CODEC_H265 = 6

        init {
            System.loadLibrary("easyrtc_media")
        }
    }
}
```

- [ ] **Step 4: Build and verify**

Run: `./gradlew :easyrtc:assembleRelease`
Expected: Build succeeds.

- [ ] **Step 5: Integrate into HomeFragment**

In `HomeFragment.kt`:

1. Add import: `import cn.easyrtc.media.VideoDecoderPipeline`
2. Remove import: `import cn.easyrtc.helper.RemoteRTCHelper`
3. Remove import: `import android.media.MediaFormat`
4. Replace field: `private var remoteRTCHelper: RemoteRTCHelper? = null` with `private var videoDecoderPipeline: VideoDecoderPipeline? = null`
5. In `onSurfaceTextureAvailable()`, remove the RemoteRTCHelper creation from the smallVideoView branch (line 235):
   Change:
   ```kotlin
   smallSurfaceTexture?.let {
       remoteRTCHelper = RemoteRTCHelper(Surface(it), MediaFormat.MIMETYPE_VIDEO_AVC, 720, 1280)
   }
   ```
   To:
   ```kotlin
   // Decoder is created lazily in ensureRemoteVideoDecoder()
   ```
6. Replace `ensureRemoteRTCHelper()` with:
   ```kotlin
   private fun ensureRemoteVideoDecoder() {
       if (videoDecoderPipeline != null) return
       val renderSurfaceTexture = if (isMainViewShowingLocal) smallSurfaceTexture else mainSurfaceTexture
       if (renderSurfaceTexture == null) {
           Log.w(TAG, "远端渲染 SurfaceTexture 未就绪，跳过 VideoDecoderPipeline 初始化")
           return
       }
       videoDecoderPipeline = VideoDecoderPipeline(Surface(renderSurfaceTexture))
   }
   ```
7. In `onTransceiverCallback()`, replace video path (line 469):
   Change: `remoteRTCHelper?.onRemoteVideoFrame(frameData)`
   To: `videoDecoderPipeline?.writeFrame(frameData, frameData.size)`
8. In `connectionStateChange()`, replace `ensureRemoteRTCHelper()` call (line 448) with `ensureRemoteVideoDecoder()`
9. In `handleVisibleResources()`, replace `ensureRemoteRTCHelper()` (line 374) with `ensureRemoteVideoDecoder()`
10. In `stopEasyRTC()`, replace cleanup (lines 551, 554):
    Change: `remoteRTCHelper?.release()` and `remoteRTCHelper = null`
    To: `videoDecoderPipeline?.release()` and `videoDecoderPipeline = null`

- [ ] **Step 6: Build and test on device**

Run: `./gradlew :app:assembleDebug`
Deploy and verify remote video decoding still works.

- [ ] **Step 7: Commit**

```bash
git add easyrtc/src/main/cpp/easyrtc_video_decoder.h easyrtc/src/main/cpp/easyrtc_video_decoder.cpp easyrtc/src/main/java/cn/easyrtc/media/VideoDecoderPipeline.kt app/src/main/java/cn/easydarwin/easyrtc/fragment/HomeFragment.kt
git commit -m "feat: migrate remote video decoding to native NDK MediaCodec pipeline"
```

---

## Task 6: Clean up legacy Java helpers

**Files:**
- Delete: `easyrtc/src/main/java/cn/easyrtc/helper/AudioHelper.kt`
- Delete: `easyrtc/src/main/java/cn/easyrtc/helper/RemoteRTCAudioHelper.kt`
- Delete: `easyrtc/src/main/java/cn/easyrtc/helper/RemoteRTCHelper.kt`
- Delete: `easyrtc/src/main/java/cn/easyrtc/helper/CameraHelper.kt`
- Delete: `easyrtc/src/main/java/cn/easyrtc/helper/CameraHelperV2.kt`
- Delete: `easyrtc/src/main/java/cn/easyrtc/helper/VideoEncoder.kt`
- Delete: `easyrtc/src/main/java/cn/easyrtc/helper/MediaHelper.kt`
- Delete: `easyrtc/src/main/java/org/easydarwin/sw/JNIUtil.java`

- [ ] **Step 1: Verify no remaining references to deleted classes**

Search the codebase for imports of the classes being deleted. If any remain (other than in the files being deleted), update them.

- [ ] **Step 2: Delete the files**

```bash
rm easyrtc/src/main/java/cn/easyrtc/helper/AudioHelper.kt
rm easyrtc/src/main/java/cn/easyrtc/helper/RemoteRTCAudioHelper.kt
rm easyrtc/src/main/java/cn/easyrtc/helper/RemoteRTCHelper.kt
rm easyrtc/src/main/java/cn/easyrtc/helper/CameraHelper.kt
rm easyrtc/src/main/java/cn/easyrtc/helper/CameraHelperV2.kt
rm easyrtc/src/main/java/cn/easyrtc/helper/VideoEncoder.kt
rm easyrtc/src/main/java/cn/easyrtc/helper/MediaHelper.kt
rm easyrtc/src/main/java/org/easydarwin/sw/JNIUtil.java
```

- [ ] **Step 3: Build and verify**

Run: `./gradlew :app:assembleDebug`
Expected: Build succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "cleanup: remove legacy Java media helpers replaced by native pipelines"
```

---

## Task 7: Final integration test

**Files:** None (testing only)

- [ ] **Step 1: Full clean build**

```bash
./gradlew clean :app:assembleDebug
```
Expected: Build succeeds.

- [ ] **Step 2: Deploy and end-to-end test**

Deploy to device/emulator. Test:
1. Start app, verify local camera preview works
2. Make a call, verify audio bidirectional communication
3. Verify remote video rendering works
4. Test camera switch
5. Test call end and cleanup

- [ ] **Step 3: Commit any remaining fixes**
