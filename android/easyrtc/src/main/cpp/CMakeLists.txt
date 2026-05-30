cmake_minimum_required(VERSION 3.22)
project(easyrtc_media)


set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)


set(REPO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../include")

add_library(easyrtc_media SHARED
    util/logger.cpp
    jni/log_bridge_jni.cpp
    codec/video_encoder.cpp
    capture/audio_capture.cpp
    codec/audio_playback.cpp
    codec/audio_decoder.cpp
    codec/video_decoder.cpp
    session/media_session.cpp
    session/transceiver_stats_reporter.cpp
    util/frame_dump.cpp
    util/frame_reader.cpp
    gl/encoder_gl_bridge.cpp
    jni/test_jni.cpp
    third_party/sonic.c
    third_party/g711.cpp
)

target_include_directories(easyrtc_media PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/util
    ${CMAKE_CURRENT_SOURCE_DIR}/session
    ${CMAKE_CURRENT_SOURCE_DIR}/codec
    ${CMAKE_CURRENT_SOURCE_DIR}/capture
    ${CMAKE_CURRENT_SOURCE_DIR}/gl
    ${CMAKE_CURRENT_SOURCE_DIR}/jni
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${REPO_INCLUDE_DIR}
)

if(MOCK_EASYRTC)
    add_library(EasyRTC STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/cpp/mock_libEasyRTC.cpp
    )
    target_include_directories(EasyRTC PRIVATE
        ${REPO_INCLUDE_DIR}
    )
    target_compile_definitions(EasyRTC PRIVATE ANDROID_BUILD)
    target_link_libraries(easyrtc_media
        android
        log
        EGL
        GLESv2
        mediandk
        camera2ndk
        aaudio
        EasyRTC
    )
else()
    target_link_libraries(easyrtc_media
        android
        log
        EGL
        GLESv2
        mediandk
        camera2ndk
        aaudio
        ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libEasyRTC.so
    )
endif()

target_compile_definitions(easyrtc_media PRIVATE ANDROID_BUILD)

set_target_properties(easyrtc_media PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
