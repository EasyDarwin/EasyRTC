#include "easyrtc_logger.h"

#include <android/log.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <fcntl.h>
#include <unistd.h>

namespace {
std::mutex gFileMutex;
const size_t kRotateBytes = 5 * 1024 * 1024;

static std::string log_dir() {
    static std::string dumpDir;
    if (!dumpDir.empty()) {
        return dumpDir;
    }
    char cmdline[256] = {0};
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0) { return ""; }
    read(fd, cmdline, sizeof(cmdline) - 1);
    close(fd);
    if (cmdline[0] == '\0') { return ""; }
    dumpDir = std::string("/sdcard/Android/data/") + cmdline + "/files";
    return dumpDir;
}

void rotate_if_needed_locked(FILE* fp) {
    if (!fp) return;
    std::fflush(fp);
    long pos = std::ftell(fp);
    if (pos < 0 || static_cast<size_t>(pos) < kRotateBytes) return;

    std::fclose(fp);
    std::remove((log_dir() + "/easyrtc_native.log.1").c_str());
    std::rename((log_dir() + "/easyrtc_native.log").c_str(), (log_dir() + "/easyrtc_native.log.1").c_str());
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

    auto kLogPath = log_dir() + "/easyrtc_native.log";
    FILE* fp = std::fopen(kLogPath.c_str(), "a+");
    if (!fp) return;

    rotate_if_needed_locked(fp);
    fp = std::fopen(kLogPath.c_str(), "a+");
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
