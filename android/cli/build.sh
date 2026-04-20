#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ -z "$ANDROID_NDK" ]; then
    if [ -n "$ANDROID_NDK_HOME" ]; then
        ANDROID_NDK="$ANDROID_NDK_HOME"
    elif [ -n "$NDK_HOME" ]; then
        ANDROID_NDK="$NDK_HOME"
    else
        echo "Error: ANDROID_NDK, ANDROID_NDK_HOME, or NDK_HOME must be set"
        exit 1
    fi
fi

TOOLCHAIN="$ANDROID_NDK/build/cmake/android.toolchain.cmake"
if [ ! -f "$TOOLCHAIN" ]; then
    echo "Error: NDK toolchain not found at $TOOLCHAIN"
    exit 1
fi

BUILD_DIR="$SCRIPT_DIR/build/arm64-v8a"
mkdir -p "$BUILD_DIR"

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "Build complete: $BUILD_DIR/easyrtc_open"
echo ""
echo "Deploy to device:"
echo "  adb push $BUILD_DIR/easyrtc_open /data/local/tmp/"
echo "  adb push $SCRIPT_DIR/jniLibs/arm64-v8a/libEasyRTC.so /data/local/tmp/"
echo "  adb shell 'cd /data/local/tmp && LD_LIBRARY_PATH=. ./easyrtc_open'"
