#ifndef EASYRTC_TRANSCEIVER_STATS_REPORTER_H
#define EASYRTC_TRANSCEIVER_STATS_REPORTER_H

#include <atomic>
#include <cstdint>
#include <thread>
#include <jni.h>

struct MediaSession;

class TransceiverStatsReporter {
public:
    explicit TransceiverStatsReporter(MediaSession* session);
    ~TransceiverStatsReporter();

    TransceiverStatsReporter(const TransceiverStatsReporter&) = delete;
    TransceiverStatsReporter& operator=(const TransceiverStatsReporter&) = delete;

    void accumulateVideoBytes(uint32_t bytes);
    void accumulateAudioBytes(uint32_t bytes);

    uint32_t videoKbpsX100() const;
    uint32_t audioKbpsX100() const;
    uint32_t totalKbpsX100() const;

private:
    void reportLoop();
    void reportKbpsStats();
    void reportFrameStats();
    void notifyKbpsStats(double videoKbps, double audioKbps, double totalKbps);
    void notifyFrameStats(uint64_t videoFrames, uint64_t audioFrames);

    const MediaSession* session_;
    std::thread thread_;
    std::atomic<bool> running_{true};

    JNIEnv* jniEnv_ = nullptr;
    jmethodID kbpsStatsMethod_ = nullptr;
    jmethodID frameStatsMethod_ = nullptr;

    std::atomic<uint64_t> videoBytesWindow_{0};
    std::atomic<uint64_t> audioBytesWindow_{0};
    std::atomic<int64_t> lastReportNs_{0};
    std::atomic<uint32_t> videoKbpsX100_{0};
    std::atomic<uint32_t> audioKbpsX100_{0};
    std::atomic<uint32_t> totalKbpsX100_{0};
};

#endif // EASYRTC_TRANSCEIVER_STATS_REPORTER_H