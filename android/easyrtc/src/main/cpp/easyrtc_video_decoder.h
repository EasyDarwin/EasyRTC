#ifndef EASYRTC_VIDEO_DECODER_H
#define EASYRTC_VIDEO_DECODER_H

#include "easyrtc_audio_playback.h"
#include "easyrtc_common.h"
#include "easyrtc_packet.hpp"
#include <android/native_window_jni.h>
#include <atomic>
#include <cstdint>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

struct VideoDecoderPipeline {

  using OnVideoSizeCallback = void (*)(void *userPtr, int width, int height);

  std::shared_ptr<AMediaCodec> decoder = nullptr;
  ANativeWindow *surface = nullptr;
  // we use the ringbuffer to replace frameQueue.
  PacketRingBuffer frameQueue{MAX_QUEUE_SIZE};
  std::atomic<bool> running{false};
  std::atomic<bool> destroyed{false};
  std::atomic<uint64_t> enqueuedFrames{0};
  std::atomic<uint64_t> renderedFrames{0};
  std::atomic<uint64_t> tryLaterCount{0};
  std::thread decodeThread;
  std::string currentCodecType;
  int width = 0;
  int height = 0;
  int frameRate = 30;
  std::atomic<int> errorCount{0};
  OnVideoSizeCallback onVideoSize = nullptr;
  void *onVideoSizeUserPtr = nullptr;
  std::vector<uint8_t> csd0;
  std::vector<uint8_t> csd1;

  int64_t audio_master_clock_us = 0;

  static constexpr int MAX_ERROR_COUNT = 5;
  static constexpr size_t MAX_QUEUE_SIZE = 30;
  static constexpr int64_t DEQUEUE_TIMEOUT_US = 10000;

  VideoDecoderPipeline() = default;
  ~VideoDecoderPipeline() {
    running.store(false);
    destroyed.store(true);

    if (decodeThread.joinable()) {
        decodeThread.join();
    }
    LOGD("VideoDecoderPipeline released");
  };
  VideoDecoderPipeline(const VideoDecoderPipeline &) = delete;
  VideoDecoderPipeline &operator=(const VideoDecoderPipeline &) = delete;
};

std::shared_ptr<VideoDecoderPipeline> videoDecoderCreate(ANativeWindow *surface, int codecType,
                                         int width, int height);
int videoDecoderStart(std::shared_ptr<VideoDecoderPipeline> pipeline);
void videoDecoderEnqueueFrame(std::shared_ptr<VideoDecoderPipeline> pipeline,
                              const uint8_t *data, int32_t size, int64_t ptsUs,
                              uint32_t frameFlags);
void videoDecoderRelease(std::shared_ptr<VideoDecoderPipeline> pipeline);

#endif
