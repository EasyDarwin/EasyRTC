#include "session/transceiver_stats_reporter.h"
#include "session/common.h"
#include "session/media_session.h"
#include <cassert>
#include <chrono>
#include <jni.h>
#include <thread>

TransceiverStatsReporter::TransceiverStatsReporter(MediaSession *session)
    : session_(session) {
  thread_ = std::thread([this]() {
    assert(session_ && "Session should not be null");
    assert(session_->jvm && "Session's JVM should not be null");
    bool shouldDetach = false;
    int ret = session_->jvm->GetEnv(reinterpret_cast<void **>(&jniEnv_),
                                    JNI_VERSION_1_6);
    if (ret == JNI_EDETACHED) {
      if (session_->jvm->AttachCurrentThread(&jniEnv_, nullptr) == JNI_OK) {
        shouldDetach = true;
      }
    }
    if (!jniEnv_) {
      LOGE("TransceiverStatsReporter: Failed to obtain JNIEnv");
      assert(false && "Failed to obtain JNIEnv");
      return;
    }
    assert(session_->javaObj && "Session's javaObj should not be null");
    jclass clazz = jniEnv_->GetObjectClass(session_->javaObj);
    assert(clazz && "clazz should not be null");
    kbpsStatsMethod_ = jniEnv_->GetMethodID(clazz, "onInputKbpsStats", "(DDD)V");
    frameStatsMethod_ =
        jniEnv_->GetMethodID(clazz, "onTransceiverFrameStats", "(JJ)V");

    LOGI("TransceiverStatsReporter thread started");
    reportLoop();
    LOGI("TransceiverStatsReporter thread exiting");

    jniEnv_ = nullptr;
    kbpsStatsMethod_ = nullptr;
    frameStatsMethod_ = nullptr;
    if (shouldDetach) {
      session_->jvm->DetachCurrentThread();
    }
  });
}

TransceiverStatsReporter::~TransceiverStatsReporter() {
  running_.store(false);
  if (thread_.joinable()) {
    thread_.join();
  }
}

void TransceiverStatsReporter::accumulateVideoBytes(uint32_t bytes) {
  videoBytesWindow_.fetch_add(bytes, std::memory_order_relaxed);
}

void TransceiverStatsReporter::accumulateAudioBytes(uint32_t bytes) {
  audioBytesWindow_.fetch_add(bytes, std::memory_order_relaxed);
}

uint32_t TransceiverStatsReporter::videoKbpsX100() const {
  return videoKbpsX100_.load(std::memory_order_relaxed);
}

uint32_t TransceiverStatsReporter::audioKbpsX100() const {
  return audioKbpsX100_.load(std::memory_order_relaxed);
}

uint32_t TransceiverStatsReporter::totalKbpsX100() const {
  return totalKbpsX100_.load(std::memory_order_relaxed);
}

void TransceiverStatsReporter::reportLoop() {
  auto lastReportTime = std::chrono::steady_clock::now();
  while (running_.load() && session_ && !session_->shuttingDown.load()) {
    if (!running_.load()) {
      break;
    }
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - lastReportTime)
                       .count();
    if (elapsed >= 1000) {
      lastReportTime = now;
      reportKbpsStats();
      reportFrameStats();
    }
    std::this_thread::sleep_until(now + std::chrono::milliseconds(100));
  }
}

void TransceiverStatsReporter::reportKbpsStats() {
  assert(session_);

  const int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
  int64_t lastNs = lastReportNs_.load(std::memory_order_relaxed);
  if (lastNs == 0) {
    lastReportNs_.compare_exchange_strong(lastNs, nowNs,
                                          std::memory_order_relaxed);
    return;
  }
  const int64_t durationNs = nowNs - lastNs;
  if (durationNs <= 0) {
    return;
  }
  if (!lastReportNs_.compare_exchange_strong(lastNs, nowNs,
                                             std::memory_order_relaxed)) {
    return;
  }

  const uint64_t videoWindowBytes =
      videoBytesWindow_.exchange(0, std::memory_order_relaxed);
  const uint64_t audioWindowBytes =
      audioBytesWindow_.exchange(0, std::memory_order_relaxed);
  const double elapsedSec = static_cast<double>(durationNs) / 1e9;
  if (elapsedSec <= 0.0) {
    return;
  }

  const double videoKbps =
      static_cast<double>(videoWindowBytes) * 8.0 / 1000.0 / elapsedSec;
  const double audioKbps =
      static_cast<double>(audioWindowBytes) * 8.0 / 1000.0 / elapsedSec;
  const double totalKbps = videoKbps + audioKbps;

  videoKbpsX100_.store(static_cast<uint32_t>(videoKbps * 100.0),
                       std::memory_order_relaxed);
  audioKbpsX100_.store(static_cast<uint32_t>(audioKbps * 100.0),
                       std::memory_order_relaxed);
  totalKbpsX100_.store(static_cast<uint32_t>(totalKbps * 100.0),
                       std::memory_order_relaxed);

  notifyKbpsStats(videoKbps, audioKbps, totalKbps);
}

void TransceiverStatsReporter::reportFrameStats() {
  assert(session_);
  const uint64_t videoFrames =
      session_->videoFramesSent.load(std::memory_order_relaxed);
  const uint64_t audioFrames =
      session_->audioFramesSent.load(std::memory_order_relaxed);

  notifyFrameStats(videoFrames, audioFrames);
}

void TransceiverStatsReporter::notifyKbpsStats(double videoKbps,
                                               double audioKbps,
                                               double totalKbps) {
  assert(jniEnv_);
  assert(session_);
  assert(session_->javaObj);
  assert(kbpsStatsMethod_);
  jniEnv_->CallVoidMethod(
      session_->javaObj, kbpsStatsMethod_, static_cast<jdouble>(videoKbps),
      static_cast<jdouble>(audioKbps), static_cast<jdouble>(totalKbps));
}

void TransceiverStatsReporter::notifyFrameStats(uint64_t videoFrames,
                                                uint64_t audioFrames) {
  assert(jniEnv_);
  assert(session_);
  assert(session_->javaObj);
  assert(frameStatsMethod_);
  jniEnv_->CallVoidMethod(session_->javaObj, frameStatsMethod_,
                          static_cast<jlong>(videoFrames),
                          static_cast<jlong>(audioFrames));
}