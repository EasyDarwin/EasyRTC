# Android Native Media Migration Design

## Goal

Migrate all media processing (transceiver creation, audio capture, audio playback, video encoding, video decoding) into native C++ within `libeasyrtc_media.so`, using the C API from `EasyRTCAPI.h` directly. Media frames never cross the JNI boundary. Only signaling remains in Java/Kotlin.

## Architecture: Native Media Session

`libeasyrtc_media.so` owns the entire media lifecycle. It receives a `peerConnection` handle (the `jlong` from `peerconnection.create()`, which is `EasyRTC_PeerConnection*`) and internally calls `EasyRTC_AddTransceiver()` with native C callbacks.

```
peerconnection.java (signaling only):
  create() → returns EasyRTC_PeerConnection* as jlong
  CreateOffer() / CreateAnswer() / SetRemoteDescription()
  AddDataChannel() / DataChannelSend() / FreeDataChannel()
  GetIceCandidateType()
  StateChange callback → Java (UI)
  IceCandidate callback → Java (WebSocket)
  DataChannel callback → Java (UI)

libeasyrtc_media.so (all media):
  Receives EasyRTC_PeerConnection* from Java
  Calls EasyRTC_AddTransceiver() directly → registers C callbacks
  Camera2 NDK → MediaCodec Encoder → EasyRTC_SendFrame()
  AAudio Record → EasyRTC_SendFrame()
  Transceiver_Callback(VIDEO) → NDK MediaCodec Decoder → Surface
  Transceiver_Callback(AUDIO) → AAudio Playback → Speaker
```

## Key Decisions

### 1. Direct C API (No dlsym, no Java Transceiver_Callback)

Use `#include "EasyRTCAPI.h"` from repo's `include/` directory. Link directly against `libEasyRTC.so`.

`EasyRTC_AddTransceiver()` takes a C function pointer `EasyRTC_Transceiver_Callback`. We register our own native function — frames stay in C++, never touch Java.

### 2. peerconnection.java simplification

Remove from `peerconnection.java`:
- `AddTransceiver` — now called natively
- `SendVideoFrame` / `SendAudioFrame` — now called natively
- `FreeTransceiver` — now called natively
- `OnEasyRTC_Transceiver_Callback` — no longer needed

Keep in `peerconnection.java`:
- `create` / `release` — PeerConnection lifecycle
- `CreateOffer` / `CreateAnswer` / `SetRemoteDescription` — SDP signaling
- `AddDataChannel` / `FreeDataChannel` / `DataChannelSend` — data channel
- `GetIceCandidateType` — connection stats
- `OnEasyRTC_ConnectionStateChange_Callback` — UI state
- `OnEasyRTC_IceCandidate_Callback` — WebSocket SDP exchange
- `OnEasyRTC_DataChannel_Callback` — UI messaging

### 3. peerconnection.create() jlong is EasyRTC_PeerConnection*

The `jlong` returned by `peerconnection.create()` IS the `EasyRTC_PeerConnection` pointer (void*). We pass it into `libeasyrtc_media.so` as context for calling `EasyRTC_AddTransceiver()` directly.

## File Structure

```
easyrtc/src/main/cpp/
  easyrtc_common.h              # Shared: logging, EasyRTCAPI.h include, constants
  easyrtc_media.h               # Existing: MediaPipeline struct (modified)
  easyrtc_media.cpp             # Existing: Camera+Encoder (modified: remove dlsym)
  easyrtc_audio.h               # NEW: AudioCapturePipeline struct
  easyrtc_audio.cpp             # NEW: AAudio recording → EasyRTC_SendFrame
  easyrtc_audio_playback.h      # NEW: AudioPlaybackPipeline struct
  easyrtc_audio_playback.cpp    # NEW: Transceiver audio callback → AAudio playback
  easyrtc_video_decoder.h       # NEW: VideoDecoderPipeline struct
  easyrtc_video_decoder.cpp     # NEW: Transceiver video callback → NDK MediaCodec decoder
  easyrtc_session.h             # NEW: Top-level MediaSession — owns all pipelines, registers transceivers
  easyrtc_session.cpp           # NEW: Creates transceivers via EasyRTC_AddTransceiver, routes callbacks
  CMakeLists.txt                # Updated: all new sources, include path, link aaudio + EasyRTC

easyrtc/src/main/java/cn/easyrtc/
  peerconnection.java           # Simplified: remove media-related native methods
  EasyRTCSdk.kt                 # Simplified: remove AddTransceiver/SendFrame, add createMediaSession
  media/MediaPipeline.kt        # Existing (unchanged)
  media/MediaSession.kt         # NEW: Thin Kotlin wrapper for native MediaSession

app/src/main/java/.../fragment/
  HomeFragment.kt               # Modified: use MediaSession for all media
```

## Core: MediaSession

### Native (easyrtc_session.h)

```cpp
struct MediaSession {
    EasyRTC_PeerConnection peerConnection = nullptr;
    EasyRTC_Transceiver videoTransceiver = nullptr;
    EasyRTC_Transceiver audioTransceiver = nullptr;

    MediaPipeline* videoEncoder = nullptr;      // camera + encoder
    AudioCapturePipeline* audioCapture = nullptr;
    AudioPlaybackPipeline* audioPlayback = nullptr;
    VideoDecoderPipeline* videoDecoder = nullptr;

    ANativeWindow* previewWindow = nullptr;
    ANativeWindow* decoderSurface = nullptr;

    std::atomic<bool> running{false};
};
```

### Native Transceiver Callback

```cpp
static int mediaTransceiverCallback(void* userPtr,
    EASYRTC_TRANSCEIVER_CALLBACK_TYPE_E type,
    EasyRTC_CODEC codecID,
    EasyRTC_Frame* frame,
    double bandwidthEstimation) {

    auto* session = static_cast<MediaSession*>(userPtr);

    switch (type) {
        case EASYRTC_TRANSCEIVER_CALLBACK_VIDEO_FRAME:
            // Feed directly to video decoder pipeline
            session->videoDecoder->enqueueFrame(frame);
            break;
        case EASYRTC_TRANSCEIVER_CALLBACK_AUDIO_FRAME:
            // Feed directly to audio playback pipeline
            session->audioPlayback->enqueueFrame(frame);
            break;
        case EASYRTC_TRANSCEIVER_CALLBACK_KEY_FRAME_REQ:
            // Request key frame on encoder
            session->videoEncoder->requestKeyFrame();
            break;
        case EASYRTC_TRANSCEIVER_CALLBACK_BANDWIDTH:
            break;
    }
    return 0;
}
```

### JNI Interface (MediaSession.kt)

```kotlin
class MediaSession {
    private var nativePtr: Long = 0

    // Called after peerconnection.create() returns the peerConnection handle
    fun create(peerConnectionHandle: Long, videoCodec: Int, audioCodec: Int): Int
    fun setEncoderConfig(codec: Int, width: Int, height: Int, bitrate: Int, fps: Int, iframeInterval: Int, cameraFacing: Int): Int
    fun setPreviewSurface(surface: Surface)
    fun setDecoderSurface(surface: Surface)
    fun start(): Int
    fun stop()
    fun switchCamera()
    fun release()

    companion object {
        init { System.loadLibrary("easyrtc_media") }
    }
}
```

### Data Flow After Migration

```
SENDING:
  Camera2 NDK → MediaCodec Encoder → EasyRTC_SendFrame(videoTransceiver) → libEasyRTC.so
  AAudio Record → EasyRTC_SendFrame(audioTransceiver) → libEasyRTC.so

RECEIVING:
  libEasyRTC.so → mediaTransceiverCallback(VIDEO) → VideoDecoderPipeline → Surface
  libEasyRTC.so → mediaTransceiverCallback(AUDIO) → AudioPlaybackPipeline → AAudio → Speaker

SIGNALING (Java):
  WebSocketManager → EasyRTCSdk → peerconnection.java → libEasyRTC.so
  libEasyRTC.so → StateChange/IceCandidate/DataChannel callbacks → Java
```

## No Kotlin Bridge Classes for Audio

Audio capture and playback are entirely internal to native. No `AudioCapturePipeline.kt` or `AudioPlaybackPipeline.kt`. The `MediaSession.kt` is the only Kotlin entry point.

## Migration Order

1. **Task 1:** Create `easyrtc_common.h` + update CMakeLists.txt (same as before)
2. **Task 2:** Clean up `easyrtc_media.cpp` — remove dlsym, use direct linking
3. **Task 3:** Create `easyrtc_audio.h/.cpp` — AAudio capture pipeline (internal, no JNI)
4. **Task 4:** Create `easyrtc_audio_playback.h/.cpp` — AAudio playback pipeline (internal, no JNI)
5. **Task 5:** Create `easyrtc_video_decoder.h/.cpp` — NDK MediaCodec decoder pipeline (internal, no JNI)
6. **Task 6:** Create `easyrtc_session.h/.cpp` — top-level MediaSession that calls `EasyRTC_AddTransceiver` with native callbacks, owns all pipelines
7. **Task 7:** Create `MediaSession.kt` — Kotlin wrapper, simplify `peerconnection.java` and `EasyRTCSdk.kt`
8. **Task 8:** Update `HomeFragment.kt` — use MediaSession instead of individual helpers
9. **Task 9:** Clean up legacy Java helpers

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
project(easyrtc_media)

set(REPO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include")

add_library(easyrtc_media SHARED
    easyrtc_media.cpp
    easyrtc_audio.cpp
    easyrtc_audio_playback.cpp
    easyrtc_video_decoder.cpp
    easyrtc_session.cpp
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

target_compile_definitions(easyrtc_media PRIVATE ANDROID_BUILD)

set_target_properties(easyrtc_media PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
```

## What Gets Deleted

- `cn.easyrtc.peerconnection` methods: AddTransceiver, SendVideoFrame, SendAudioFrame, FreeTransceiver, OnEasyRTC_Transceiver_Callback
- `cn.easyrtc.helper.AudioHelper`
- `cn.easyrtc.helper.RemoteRTCAudioHelper`
- `cn.easyrtc.helper.RemoteRTCHelper`
- `cn.easyrtc.helper.CameraHelper`, `CameraHelperV2`
- `cn.easyrtc.helper.VideoEncoder`
- `cn.easyrtc.helper.MediaHelper` (may keep for UI codec enumeration)
- `org.easydarwin.sw.JNIUtil` (only used by legacy CameraHelper)
