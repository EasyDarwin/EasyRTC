#ifndef EASYRTC_COMMON_H
#define EASYRTC_COMMON_H

#include "EasyRTCAPI.h"
#include "easyrtc_logger.h"

#define LOG_TAG "EasyRTCMedia"
#define LOGD(...) easyrtc_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) easyrtc_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) easyrtc_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

#define COLOR_FormatSurface 0x7f420888
#define BUFFER_FLAG_CODEC_CONFIG 2

#endif
