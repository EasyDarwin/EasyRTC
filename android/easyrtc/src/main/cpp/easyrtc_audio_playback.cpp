#include "easyrtc_audio_playback.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

static float computePlaybackSpeed(int bufferedSamples) {
  float bufferedMs = static_cast<float>(bufferedSamples) * 1000.0f /
                     static_cast<float>(AudioPlaybackPipeline::SAMPLE_RATE *
                                        AudioPlaybackPipeline::CHANNEL_COUNT);
  if (bufferedMs > AudioPlaybackPipeline::SPEED_UP_THRESHOLD_MS) {
    float excess = bufferedMs - AudioPlaybackPipeline::SPEED_UP_THRESHOLD_MS;
    float speed = 1.0f + excess / AudioPlaybackPipeline::SPEED_UP_THRESHOLD_MS;
    return std::min(speed, AudioPlaybackPipeline::MAX_SPEED);
  }
  return 1.0f;
}

static aaudio_data_callback_result_t playbackCallback(AAudioStream *stream,
                                                      void *userData,
                                                      void *audioData,
                                                      int32_t numFrames) {
  auto *pipeline = static_cast<AudioPlaybackPipeline *>(userData);
  if (!pipeline || pipeline->stopped.load()) {
    return AAUDIO_CALLBACK_RESULT_STOP;
  }

  int32_t requestedBytes =
      numFrames * pipeline->CHANNEL_COUNT * sizeof(int16_t);
  auto *output = static_cast<uint8_t *>(audioData);

  std::lock_guard<std::mutex> lock(pipeline->mutex_);
  int32_t avail = sonicSamplesAvailable(pipeline->sonicStream.get());
  auto millis = avail * 1000.0f /
                static_cast<float>(AudioPlaybackPipeline::SAMPLE_RATE *
                                   AudioPlaybackPipeline::CHANNEL_COUNT);
  if (pipeline->lack_of_pcm_ && (avail < numFrames * pipeline->CHANNEL_COUNT || millis < 80.0f)) { // we need at least 80ms data to recover from underrun
    memset(output, 0, requestedBytes);
    requestedBytes = 0;
  }

  if (requestedBytes > 0) {
    int32_t requestedSamples =
        requestedBytes / static_cast<int32_t>(sizeof(int16_t));
    int32_t totalReadSamples = 0;
    while (totalReadSamples < requestedSamples) {
      int32_t availNow = sonicSamplesAvailable(pipeline->sonicStream.get());
      if (availNow == 0)
        break;

      int32_t toRead = std::min(availNow, requestedSamples - totalReadSamples);
      int32_t read = sonicReadShortFromStream(
          pipeline->sonicStream.get(),
          reinterpret_cast<int16_t *>(output) + totalReadSamples, toRead);
      if (read <= 0)
        break;
      totalReadSamples += read;
    }

    int32_t consumedBytes =
        totalReadSamples * static_cast<int32_t>(sizeof(int16_t));
    requestedBytes -= consumedBytes;
    output += consumedBytes;

    if (pipeline->lack_of_pcm_) {
      if (requestedBytes == 0) {
        LOGW("[AUDIO] SonicRecovery: data is back");
        pipeline->lack_of_pcm_ = false;
      }
      // do fade in for the first 10ms to avoid audio crackle
      int32_t fadeInBytes = std::min(static_cast<int32_t>(totalReadSamples * sizeof(int16_t)),
                                     static_cast<int32_t>(AudioPlaybackPipeline::SAMPLE_RATE * pipeline->CHANNEL_COUNT *
                                                        sizeof(int16_t) / 100)); // 10ms fade in
      assert(fadeInBytes % 2 == 0);
      auto fadeInSamples = fadeInBytes / sizeof(int16_t);
      auto _pcm = static_cast<uint8_t *>(audioData);
      auto fadeInStart = reinterpret_cast<int16_t *>(_pcm);

      LOGW("[AUDIO] SonicUnderrun: recver from lack of pcm with %d samples, fade in %d samples", requestedBytes/sizeof(int16_t), fadeInBytes/sizeof(int16_t));
      for (int32_t i = 0; i < fadeInSamples; i++) {
        float ratio = static_cast<float>(i) / fadeInSamples;
        fadeInStart[i] = static_cast<int16_t>(fadeInStart[i] * ratio);
      }
    }
  }

  if (requestedBytes > 0) {
    pipeline->lack_of_pcm_ = true;
    auto _pcm = static_cast<uint8_t *>(audioData);
    // fade out the last 10ms to avoid audio crackle and then, fill the rest with silence
    int32_t fadeOutBytes = std::min(static_cast<int32_t>(output - _pcm), static_cast<int32_t>(
        AudioPlaybackPipeline::SAMPLE_RATE * pipeline->CHANNEL_COUNT *
        sizeof(int16_t) / 100)); // 10ms fade out
    assert(fadeOutBytes % 2 == 0);
    auto fadeOutStart = output - fadeOutBytes;
    assert(fadeOutStart - _pcm >= 0l);
    if (fadeOutStart - _pcm > 0) {
      LOGW("[AUDIO] SonicUnderrun: filling silence %d samples, fade out %d samples", requestedBytes/sizeof(int16_t), fadeOutBytes/sizeof(int16_t));
      
      assert((fadeOutStart - _pcm) % 2 == 0);
      const auto fadeOutSamples = fadeOutBytes / sizeof(int16_t);
      auto fadeOutStartSamples = reinterpret_cast<int16_t *>(fadeOutStart);
      
      for (int32_t i = 0; i < fadeOutSamples; i++) {
        float ratio = 1.0f - static_cast<float>(i) / fadeOutSamples;
        fadeOutStartSamples[i] = static_cast<int16_t>(fadeOutStartSamples[i] * ratio);
      }
    }
    memset(output, 0, requestedBytes);
  }
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void playbackErrorCallback(AAudioStream *stream, void *userData,
                                  aaudio_result_t error) {
  LOGE("AAudio playback error: %d", error);
}

static int
ensureStreamCreated(std::shared_ptr<AudioPlaybackPipeline> pipeline) {
  if (pipeline->stream)
    return 0;

  LOGI("[AUDIO] AudioPlaybackOpen: %dHz %dch",
       AudioPlaybackPipeline::SAMPLE_RATE,
       AudioPlaybackPipeline::CHANNEL_COUNT);

  AAudioStreamBuilder *builder = nullptr;
  aaudio_result_t result = AAudio_createStreamBuilder(&builder);
  if (result != AAUDIO_OK || !builder) {
    LOGE("Failed to create AAudio stream builder: %d", result);
    return -1;
  }

  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setSampleRate(builder,
                                    AudioPlaybackPipeline::SAMPLE_RATE);
  AAudioStreamBuilder_setChannelCount(builder,
                                      AudioPlaybackPipeline::CHANNEL_COUNT);
  AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
  AAudioStreamBuilder_setPerformanceMode(builder,
                                         AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_VOICE_COMMUNICATION);
  AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_SPEECH);
  AAudioStreamBuilder_setDataCallback(builder, playbackCallback,
                                      pipeline.get());
  AAudioStreamBuilder_setErrorCallback(builder, playbackErrorCallback,
                                       pipeline.get());

  AAudioStream *stream = nullptr;
  result = AAudioStreamBuilder_openStream(builder, &stream);
  AAudioStreamBuilder_delete(builder);

  if (result != AAUDIO_OK || !stream) {
    LOGE("Failed to open AAudio output stream: %d", result);
    return -1;
  }

  pipeline->stream = stream;
  pipeline->stopped.store(false);

  result = AAudioStream_requestStart(stream);
  if (result != AAUDIO_OK) {
    LOGE("Failed to start AAudio output stream: %d", result);
    AAudioStream_close(stream);
    pipeline->stream = nullptr;
    return -1;
  }

  pipeline->playing.store(true);
  LOGI("[AUDIO] AudioPlaybackOpen: SUCCESS stream=%p", stream);
  return 0;
}

std::shared_ptr<AudioPlaybackPipeline> audioPlaybackCreate(int audioCodec) {
  auto pipeline = std::make_shared<AudioPlaybackPipeline>();
  pipeline->audioDecoder = audioDecoderCreate(audioCodec);
  pipeline->sonicStream = std::shared_ptr<sonicStreamStruct>(
      sonicCreateStream(AudioPlaybackPipeline::SAMPLE_RATE,
                        AudioPlaybackPipeline::CHANNEL_COUNT),
      sonicDestroyStream);
  LOGI("[AUDIO] AudioPlaybackCreate: sonic=%p", pipeline->sonicStream.get());
  return pipeline;
}

void audioPlaybackEnqueueFrame(std::shared_ptr<AudioPlaybackPipeline> pipeline,
                               const uint8_t *data, int32_t size) {
  if (!pipeline || pipeline->stopped.load() || size <= 0)
    return;

  std::vector<int16_t> pcm =
      audioDecoderDecode(pipeline->audioDecoder, data, size);
  if (pcm.empty())
    return;

  if (!pipeline->playing.load()) {
    ensureStreamCreated(pipeline);
  }

  #if 0
  {
    // test sleep to simulate slow decoding and see if sonic can speed up the playback
    static int64_t _idx = 0;
    if (_idx++ % 500 == 0) {
      LOGI("[AUDIO] Simulating slow decoding...");
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
  }
  #endif

  std::lock_guard<std::mutex> lock(pipeline->mutex_);
  int32_t avail = sonicSamplesAvailable(pipeline->sonicStream.get());
  auto millis = avail * 1000.0f /
                static_cast<float>(AudioPlaybackPipeline::SAMPLE_RATE *
                                   AudioPlaybackPipeline::CHANNEL_COUNT);
  if (millis > AudioPlaybackPipeline::SPEED_UP_THRESHOLD_MS) {
    float speed = computePlaybackSpeed(avail);
    if (speed != pipeline->currentSpeed) {
      LOGI("[AUDIO] SonicSpeedChange: %.2f -> %.2f, bufferedSamples=%d",
           pipeline->currentSpeed, speed, avail);
      pipeline->currentSpeed = speed;
      sonicSetSpeed(pipeline->sonicStream.get(), speed);
    }
  }
  int samples = static_cast<int>(pcm.size());
  int written =
      sonicWriteShortToStream(pipeline->sonicStream.get(), pcm.data(), samples);
  {
    // loging every 500 frames to avoid flooding the logcat
    static int64_t __idx = 0;
    if (__idx++ % 500 == 0) {
      int buffered = sonicSamplesAvailable(pipeline->sonicStream.get());
      LOGI("[AUDIO] SonicEnqueue: wrote samples=%d buffered samples=%d "
           "speed=%.2f",
           written, buffered, pipeline->currentSpeed);
    }
  }
}

void audioPlaybackRelease(std::shared_ptr<AudioPlaybackPipeline> pipeline) {
  if (!pipeline)
    return;

  LOGI("[AUDIO] AudioPlaybackClose: stream=%p", pipeline->stream);

  pipeline->stopped.store(true);
  pipeline->playing.store(false);

  if (pipeline->stream) {
    AAudioStream_requestStop(pipeline->stream);
    AAudioStream_close(pipeline->stream);
    pipeline->stream = nullptr;
  }
  LOGI("[AUDIO] AudioPlaybackClose: DONE");
}