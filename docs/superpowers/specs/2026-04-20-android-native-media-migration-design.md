# Android Native Media Migration Design

## Goal

Migrate audio capture, remote audio playback, and remote video decoding from Java/Kotlin to native C++ within the existing `libeasyrtc_media.so`, following the pattern established by the camera+encoder pipeline.

## Architecture: Multi-.cpp, Single .so

All native media code compiles into one `libeasyrtc_media.so`. Each module gets its own `.cpp`/`.h` pair. A shared header provides common utilities.

```
easyrtc/src/main/cpp/
  easyrtc_common.h           # Shared: logging macros, EasyRTCAPI.h include, common types
  easyrtc_media.h/.cpp       # Existing: Camera + Encoder pipeline
  easyrtc_audio.h/.cpp       # NEW: Audio capture pipeline (AAudio + AEC)
  easyrtc_audio_playback.h/.cpp  # NEW: Remote audio playback pipeline (AAudio)
  easyrtc_video_decoder.h/.cpp   # NEW: Remote video decoder pipeline (NDK MediaCodec)
  CMakeLists.txt             # Updated: add new sources, link libEasyRTC
```

## Key Decision: Direct Linking (No dlsym)

Use `#include "EasyRTCAPI.h"` from the repo's `include/` directory and link directly against `libEasyRTC.so`. This eliminates the existing `dlopen/dlsym` hack in `easyrtc_media.cpp`. The existing dlsym code should be cleaned up as part of this migration.

CMakeLists.txt will:
- Add `../../include` as include directory (or a relative path to the repo root `include/`)
- Add `target_link_libraries(easyrtc_media EasyRTC)` using the prebuilt .so from jniLibs

## Migration Order

### Step 1: Audio Capture (AudioHelper -> native)

**Java source:** `cn.easyrtc.helper.AudioHelper` (176 lines)
**New native files:** `easyrtc_audio.h`, `easyrtc_audio.cpp`
**New Kotlin bridge:** `cn.easyrtc.media.AudioCapturePipeline`

**AudioCapturePipeline struct:**
```
struct AudioCapturePipeline {
    AAudioStream* stream;
    EasyRTC_Transceiver audioTransceiver;
    std::atomic<bool> running;
    pthread_mutex_t mutex;
};
```

**API:** AAudio (requires API 27+, confirmed acceptable)
**Format:** 8kHz, mono, PCM 16-bit, 20ms frames (320 bytes)
**AEC:** Integrate `libaecm-shared.so` directly in native layer
**Audio source:** VOICE_RECOGNITION (matching current Java behavior)

**JNI interface (AudioCapturePipeline.kt):**
- `nativeCreate(transceiverHandle: Long): Long`
- `nativeStart(pipelinePtr: Long): Int`
- `nativeStop(pipelinePtr: Long)`
- `nativeRelease(pipelinePtr: Long)`

**Data flow:**
```
AAudio recording callback -> PCM 20ms frame -> AEC processing -> EasyRTC_SendFrame() -> libEasyRTC.so
```

### Step 2: Remote Audio Playback (RemoteRTCAudioHelper -> native)

**Java source:** `cn.easyrtc.helper.RemoteRTCAudioHelper` (210 lines)
**New native files:** `easyrtc_audio_playback.h`, `easyrtc_audio_playback.cpp`
**New Kotlin bridge:** `cn.easyrtc.media.AudioPlaybackPipeline`

**AudioPlaybackPipeline struct:**
```
struct AudioPlaybackPipeline {
    AAudioStream* stream;
    std::queue<std::vector<uint8_t>> jitterBuffer;
    std::mutex queueMutex;
    std::condition_variable queueCv;
    std::atomic<bool> playing;
    std::atomic<bool> stopped;
    static constexpr size_t MAX_QUEUE_SIZE = 200;
};
```

**API:** AAudio (output stream)
**Format:** 8kHz, mono, PCM 16-bit
**Jitter buffer:** C++ queue, max 200 frames, drop oldest on overflow
**Silence:** Write silence frame (320 zero bytes) when queue empty

**JNI interface (AudioPlaybackPipeline.kt):**
- `nativeCreate(): Long`
- `nativeWriteFrame(pipelinePtr: Long, data: ByteArray, size: Int)`
- `nativeRelease(pipelinePtr: Long)`

**Data flow:**
```
libEasyRTC.so audio callback -> Kotlin bridge -> nativeWriteFrame -> jitter queue -> AAudio playback callback -> speaker
```

### Step 3: Remote Video Decoding (RemoteRTCHelper -> native)

**Java source:** `cn.easyrtc.helper.RemoteRTCHelper` (372 lines)
**New native files:** `easyrtc_video_decoder.h`, `easyrtc_video_decoder.cpp`
**New Kotlin bridge:** `cn.easyrtc.media.VideoDecoderPipeline`

**VideoDecoderPipeline struct:**
```
struct VideoDecoderPipeline {
    AMediaCodec* decoder;
    ANativeWindow* surface;
    std::queue<std::vector<uint8_t>> frameQueue;
    std::mutex queueMutex;
    std::atomic<bool> running;
    std::atomic<bool> destroyed;
    pthread_t decodeThread;
    std::string currentCodecType;  // "video/avc" or "video/hevc"
    int width;
    int height;
    int frameRate;
    std::atomic<int> errorCount;
    static constexpr int MAX_ERROR_COUNT = 5;
    static constexpr size_t MAX_QUEUE_SIZE = 30;
};
```

**API:** NDK MediaCodec decoder (AMediaCodec)
**Codec support:** H.264 (video/avc), H.265 (video/hevc)
**Error recovery:** Auto-reinit decoder after 5 consecutive errors
**Codec switching:** Support runtime codec type change

**JNI interface (VideoDecoderPipeline.kt):**
- `nativeCreate(surface: Surface, codecType: Int, width: Int, height: Int): Long`
- `nativeWriteFrame(pipelinePtr: Long, data: ByteArray, size: Int)`
- `nativeReinit(pipelinePtr: Long, codecType: Int)`
- `nativeRelease(pipelinePtr: Long)`

**Data flow:**
```
libEasyRTC.so video callback -> Kotlin bridge -> nativeWriteFrame -> frame queue -> decodeThread (AMediaCodec decoder) -> Surface render
```

## Cleanup: Remove dlsym from existing code

**File:** `easyrtc_media.cpp`
- Remove `dlopen`/`dlsym`/`dlclose` code (lines 31-61)
- Remove local `EasyRTC_Frame` typedef (use `EasyRTCAPI.h`)
- Remove local `sendFrame` wrapper function
- Replace `sendFrame(realTransceiver, &frame)` with `EasyRTC_SendFrame(realTransceiver, &frame)`
- Add `#include "EasyRTCAPI.h"` (via `easyrtc_common.h`)

**File:** `CMakeLists.txt`
- Add include path for `include/` directory
- Add `target_link_libraries` for `EasyRTC`

## Shared Header: easyrtc_common.h

```cpp
#ifndef EASYRTC_COMMON_H
#define EASYRTC_COMMON_H

#include <android/log.h>
#include "EasyRTCAPI.h"

#define LOG_TAG "EasyRTCMedia"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

#endif
```

## CMakeLists.txt Changes

```cmake
# Add include path for EasyRTCAPI.h
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../include)

add_library(easyrtc_media SHARED
    easyrtc_media.cpp
    easyrtc_audio.cpp
    easyrtc_audio_playback.cpp
    easyrtc_video_decoder.cpp
)

# Link against prebuilt libEasyRTC.so
find_library(EASYRTC_LIB EasyRTC PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})
target_link_libraries(easyrtc_media
    ${EASYRTC_LIB}
    log
    android
    camera2ndk
    mediandk
    aaudio
)
```

## Kotlin Bridge Classes

All follow the same handle-based pattern as existing `MediaPipeline.kt`:
- Store native pointer as `Long`
- `init` / `finalize` lifecycle
- Thin wrappers around JNI native methods

## Final Architecture After Migration

```
Native Layer (libeasyrtc_media.so):
  Camera2 NDK -> MediaCodec Encoder -> EasyRTC_SendFrame -> libEasyRTC.so
  AAudio Record -> AEC(libaecm) -> EasyRTC_SendFrame -> libEasyRTC.so
  libEasyRTC.so -> VideoDecoderPipeline -> Surface
  libEasyRTC.so -> AudioPlaybackPipeline -> AAudio -> Speaker

Java/Kotlin Layer (unchanged):
  WebSocketManager (OkHttp) -> signaling
  EasyRTCSdk -> JNI bridge to libEasyRTC.so
  HomeFragment -> UI + native pipeline orchestration
```

## What Gets Deleted After Migration

- `cn.easyrtc.helper.AudioHelper` - replaced by native AudioCapturePipeline
- `cn.easyrtc.helper.RemoteRTCAudioHelper` - replaced by native AudioPlaybackPipeline
- `cn.easyrtc.helper.RemoteRTCHelper` - replaced by native VideoDecoderPipeline
- `cn.easyrtc.helper.CameraHelper` - legacy, already unused
- `cn.easyrtc.helper.CameraHelperV2` - legacy stub
- `cn.easyrtc.helper.VideoEncoder` - legacy, already unused
- `cn.easyrtc.helper.MediaHelper` - codec enumeration, may still be needed for UI
- `org.easydarwin.sw.JNIUtil` - only used by legacy CameraHelper path
