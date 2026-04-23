#include "easyrtc_logger.h"

#include <android/log.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>

namespace {
std::mutex gFileMutex;
const char* kLogPath = "/data/local/tmp/easyrtc_native.log";
const size_t kRotateBytes = 5 * 1024 * 1024;

void rotate_if_needed_locked(FILE* fp) {
    if (!fp) return;
    std::fflush(fp);
    long pos = std::ftell(fp);
    if (pos < 0 || static_cast<size_t>(pos) < kRotateBytes) return;

    std::fclose(fp);
    std::remove("/data/local/tmp/easyrtc_native.log.1");
    std::rename(kLogPath, "/data/local/tmp/easyrtc_native.log.1");
}

const char* level_tag(int priority) {
    switch (priority) {
        case ANDROID_LOG_ERROR: return "E";
        case ANDROID_LOG_WARN: return "W";
        case ANDROID_LOG_INFO: return "I";
        default: return "D";
    }
}

void write_file_line(int priority, const char* tag, const char* message) {
    std::lock_guard<std::mutex> lock(gFileMutex);

    FILE* fp = std::fopen(kLogPath, "a+");
    if (!fp) return;

    rotate_if_needed_locked(fp);
    fp = std::fopen(kLogPath, "a+");
    if (!fp) return;

    std::time_t now = std::time(nullptr);
    std::tm tm_buf{};
    localtime_r(&now, &tm_buf);
    char ts[32] = {0};
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm_buf);

    std::fprintf(fp, "%s [%s] %s: %s\n", ts, level_tag(priority), tag, message);
    std::fclose(fp);
}
}

void easyrtc_log_print(int priority, const char* tag, const char* fmt, ...) {
    char buf[2048] = {0};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    __android_log_print(priority, tag, "%s", buf);
    write_file_line(priority, tag, buf);
}
