# Android CLI for EasyRTC - Design

## Goal

Port the Linux EasyRTC CLI tool to Android as a native executable (arm64-v8a), replicating `linux/EasyRTC_Open/EasyRTC_Open/main.c` behavior: register as device or connect as caller, receive media, save to files.

## Architecture

```
android/cli/
├── CMakeLists.txt           # Builds easyrtc_open executable
├── build.sh                 # Build helper script
├── jni/                     # Stub JNI header for ANDROID ifdef compatibility
│   └── JNI_EasyRTCDevice.h
└── jniLibs/arm64-v8a/
    └── libEasyRTC.so        # Prebuilt (copied from android/easyrtc/...)
```

No modifications to existing source files under `linux/`.

## Source Files Compiled

All from existing code, compiled with `-DANDROID`:

| Source | Origin |
|--------|--------|
| EasyRTCDeviceAPI.c | linux/EasyRTC_Open/EasyRTC_Open/ |
| RTCDevice.c | linux/EasyRTC_Open/EasyRTC_Open/ |
| RTCPeer.c | linux/EasyRTC_Open/EasyRTC_Open/ |
| RTCCaller.c | linux/EasyRTC_Open/EasyRTC_Open/ |
| WsHandle.c | linux/EasyRTC_Open/EasyRTC_Open/ |
| EasyRTC_websocket.c | linux/EasyRTC_Open/EasyRTC_Open/ |
| websocketClient.c | linux/EasyRTC_Open/EasyRTC_Open/ |
| g711.c | linux/EasyRTC_Open/EasyRTC_Open/ |
| main.c | linux/EasyRTC_Open/EasyRTC_Open/ |
| buff.c | linux/common/ |
| osmutex.c | linux/common/ |
| osthread.c | linux/common/ |

## Platform Adaptations

### Stub JNI Header
The source has `#ifdef ANDROID` blocks that include `jni/JNI_EasyRTCDevice.h` and call `LOGD`. A stub header at `android/cli/jni/JNI_EasyRTCDevice.h` will define `LOGD` as a no-op macro so the original source compiles without modification.

### Signal Handling
Android bionic supports `SIGPIPE`, `SIGTERM`, `SIGINT`, `SIGQUIT` — no changes needed.

### DNS Resolution
`gethostbyname` works on Android — no changes needed.

### TCP Keepalive
The source already handles Android TCP keepalive constants via `#ifdef ANDROID` guards in `websocketClient.h`.

## Build

```bash
cd android/cli
mkdir -p build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-21 \
      ..
make
```

Output: `easyrtc_open` executable.

## Usage

```bash
adb push easyrtc_open /data/local/tmp/
adb push libEasyRTC.so /data/local/tmp/
adb shell "cd /data/local/tmp && chmod +x easyrtc_open && LD_LIBRARY_PATH=. ./easyrtc_open <serverAddr> <serverPort> <deviceUUID>"
```

## Dependencies

- Android NDK (r21+)
- Prebuilt `libEasyRTC.so` (arm64-v8a) from `android/easyrtc/src/main/jniLibs/arm64-v8a/`
- `libc` (bionic), `libm`, `libpthread` (all provided by NDK)

## ABI Target

arm64-v8a only (matching available prebuilt `libEasyRTC.so`).
