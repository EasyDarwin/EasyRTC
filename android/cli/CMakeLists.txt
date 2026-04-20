cmake_minimum_required(VERSION 3.22)
project(easyrtc_open C)

set(EASYRTC_OPEN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/EasyRTC_Open/EasyRTC_Open/EasyRTCDeviceAPI.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/EasyRTC_Open/EasyRTC_Open/RTCDevice.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/EasyRTC_Open/EasyRTC_Open/RTCPeer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/EasyRTC_Open/EasyRTC_Open/RTCCaller.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/EasyRTC_Open/EasyRTC_Open/WsHandle.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/EasyRTC_Open/EasyRTC_Open/EasyRTC_websocket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/EasyRTC_Open/EasyRTC_Open/websocketClient.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/EasyRTC_Open/EasyRTC_Open/g711.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/EasyRTC_Open/EasyRTC_Open/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/common/buff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/common/osmutex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/common/osthread.c
)

add_executable(easyrtc_open ${EASYRTC_OPEN_SRC})

target_include_directories(easyrtc_open PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/EasyRTC_Open/EasyRTC_Open
    ${CMAKE_CURRENT_SOURCE_DIR}/../../linux/common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/jni
)

target_compile_definitions(easyrtc_open PRIVATE
    ANDROID
    ANDROID_BUILD
)

target_link_libraries(easyrtc_open
    ${CMAKE_CURRENT_SOURCE_DIR}/jniLibs/${ANDROID_ABI}/libEasyRTC.so
    log
)
