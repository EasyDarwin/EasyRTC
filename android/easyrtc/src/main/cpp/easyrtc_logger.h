#ifndef EASYRTC_LOGGER_H
#define EASYRTC_LOGGER_H

#include <android/log.h>

void easyrtc_log_print(int priority, const char* tag, const char* fmt, ...);

#endif
