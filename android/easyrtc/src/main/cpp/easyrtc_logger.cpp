#include "easyrtc_logger.h"

#include <android/log.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/details/thread_pool.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <fcntl.h>
#include <unistd.h>

namespace {
constexpr size_t kRotateBytes = 20 * 1024 * 1024;
constexpr size_t kRotateFiles = 2;
constexpr size_t kAsyncQueueSize = 8192;
constexpr size_t kAsyncWorkerThreads = 1;

std::shared_ptr<spdlog::details::thread_pool> get_thread_pool() {
    static std::shared_ptr<spdlog::details::thread_pool> tp = []() {
        spdlog::init_thread_pool(kAsyncQueueSize, kAsyncWorkerThreads);
        return spdlog::thread_pool();
    }();
    return tp;
}

std::string log_dir() {
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

spdlog::level::level_enum to_spdlog_level(int priority) {
    switch (priority) {
        case ANDROID_LOG_VERBOSE: return spdlog::level::trace;
        case ANDROID_LOG_DEBUG: return spdlog::level::debug;
        case ANDROID_LOG_INFO: return spdlog::level::info;
        case ANDROID_LOG_WARN: return spdlog::level::warn;
        case ANDROID_LOG_ERROR: return spdlog::level::err;
        case ANDROID_LOG_FATAL: return spdlog::level::critical;
        default: return spdlog::level::debug;
    }
}

std::shared_ptr<spdlog::logger> create_logger() {
    std::vector<spdlog::sink_ptr> sinks;

    auto androidSink = std::make_shared<spdlog::sinks::android_sink_mt>("EasyRTC.Native", true);
    androidSink->set_pattern("[%l] [pid:%P tid:%t] %v");
    sinks.push_back(androidSink);

    const std::string dir = log_dir();
    if (!dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (!ec) {
            const auto filePath = dir + "/easyrtc_native.log";
            auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                filePath,
                kRotateBytes,
                kRotateFiles,
                true);
            fileSink->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [pid:%P tid:%t] %v");
            sinks.push_back(fileSink);
        }
    }

    auto logger = std::make_shared<spdlog::async_logger>(
        "easyrtc_native",
        sinks.begin(),
        sinks.end(),
        get_thread_pool(),
        spdlog::async_overflow_policy::block);
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::info);
    return logger;
}

std::shared_ptr<spdlog::logger> get_logger() {
    static std::shared_ptr<spdlog::logger> logger = create_logger();
    return logger;
}

void fallback_logcat(int priority, const char* tag, const char* message) {
    __android_log_print(priority,
                        tag ? tag : "EasyRTC.Native",
                        "[pid:%d tid:%d] %s",
                        static_cast<int>(getpid()),
                        static_cast<int>(gettid()),
                        message ? message : "");
}
}

void easyrtc_log_print(int priority, const char* tag, const char* fmt, ...) {
    char buf[2048] = {0};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    try {
        auto logger = get_logger();
        logger->log(to_spdlog_level(priority), "[{}] {}", tag ? tag : "EasyRTC.Native", buf);
    } catch (...) {
        fallback_logcat(priority, tag, buf);
    }
}
