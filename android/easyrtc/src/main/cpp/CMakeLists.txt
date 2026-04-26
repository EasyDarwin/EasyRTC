cmake_minimum_required(VERSION 3.22)
project(easyrtc_media)

set(REPO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../include")

add_library(easyrtc_media SHARED
    easyrtc_logger.cpp
    easyrtc_media.cpp
    easyrtc_audio.cpp
    easyrtc_audio_playback.cpp
    easyrtc_audio_decoder.cpp
    easyrtc_video_decoder.cpp
    easyrtc_session.cpp
    easyrtc_frame_dump.cpp
    easyrtc_frame_reader.cpp
    easyrtc_test_jni.cpp
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
