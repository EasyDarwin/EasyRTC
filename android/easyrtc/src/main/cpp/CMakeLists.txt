cmake_minimum_required(VERSION 3.22)
project(easyrtc_media)

add_library(easyrtc_media SHARED easyrtc_media.cpp)

target_include_directories(easyrtc_media PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(easyrtc_media
    android
    log
    mediandk
    camera2ndk
    ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libEasyRTC.so)

set_target_properties(easyrtc_media PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
