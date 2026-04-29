#include "easyrtc_audio_playback.h"
#include "easyrtc_common.h"
#include "sonic.h"
#include <aaudio/AAudio.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

#define OUT_LACK_OF_PCM_MODE_WATER_LEVEL_MS 200.0f

static aaudio_data_callback_result_t playbackCallback(AAudioStream *stream,
                                                      void *userData,
                                                      void *audioData,
                                                      int32_t numFrames) {
  auto *pipeline = static_cast<AudioPlaybackPipeline *>(userData);
  if (!pipeline || pipeline->stopped.load()) {
    LOGW("[AUDIO] play callback with pipeline stopped or null, stopping "
         "playback");
    return AAUDIO_CALLBACK_RESULT_STOP;
  }
  const int32_t numSamples = numFrames * pipeline->CHANNEL_COUNT;
  int32_t requestedBytes = numSamples * sizeof(int16_t);
  auto *output = static_cast<uint8_t *>(audioData);

  std::lock_guard<std::mutex> lock(pipeline->mutex_);
  int32_t avail = sonicSamplesAvailable(pipeline->sonicStream.get());
  auto millis = avail * 1000.0f /
                static_cast<float>(AudioPlaybackPipeline::SAMPLE_RATE *
                                   AudioPlaybackPipeline::CHANNEL_COUNT);
  FLOGI("[AUDIO] play callback with numFrames:%d, available samples %d, "
        "buffered %f ms",
        numFrames, avail, millis);
  bool exitFromLackOfPcmState = false;
  if (pipeline->lack_of_pcm_ && millis > OUT_LACK_OF_PCM_MODE_WATER_LEVEL_MS && avail > numSamples) {
    pipeline->lack_of_pcm_ = false;
    exitFromLackOfPcmState = true;
  }

  if (pipeline->lack_of_pcm_) {
    memset(output, 0, requestedBytes);
    LOGI("[AUDIO] in lack_of_pcm state, ignore %d request, with available:%d "
         "in buffer",
         requestedBytes, avail);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }
  int32_t readBytesOffset = 0;
  { // sonic to output
    int32_t read = sonicReadShortFromStream(pipeline->sonicStream.get(),
                                            reinterpret_cast<int16_t *>(output),
                                            numSamples);
    assert(read <= numSamples);
    readBytesOffset = read * sizeof(int16_t);

    if (read < numSamples) {
      pipeline->lack_of_pcm_ = true;
      // apply a fadeOut
      int fadeOutFrames = std::min(read, AudioPlaybackPipeline::SAMPLE_RATE /
                                             100); // 10ms fade out
      auto _pcm = static_cast<int16_t *>(audioData);
      auto fadeOutStart = _pcm + read - fadeOutFrames;
      for (int32_t i = 0; i < fadeOutFrames; i++) {
        float ratio = static_cast<float>(fadeOutFrames - i) / fadeOutFrames;
        fadeOutStart[i] = static_cast<int16_t>(fadeOutStart[i] * ratio);
      }
      LOGI("[AUDIO] Enter lack_of_pcm state, read %d "
           "samples, requested %d samples, fadeOut applied for last %d frames",
           read, numSamples, fadeOutFrames);
      memset(output + readBytesOffset, 0, requestedBytes - readBytesOffset);
    } else if (exitFromLackOfPcmState) {
      // do fade in for the first 10ms to avoid audio crackle
      int32_t fadeInFrames = std::min(
          numFrames, AudioPlaybackPipeline::SAMPLE_RATE / 100); // 10ms fade in
      auto const fadeInBytes =
          fadeInFrames * pipeline->CHANNEL_COUNT * sizeof(int16_t);
      assert(fadeInBytes % 2 == 0);
      auto _pcm = static_cast<int16_t *>(audioData);
      auto fadeInStart = _pcm;
    LOGW("[AUDIO] Exit lack_of_pcm state. available:%d, numSamples:%d, FadeIn with %d frames",
         avail, numSamples, fadeInFrames);
      for (int32_t i = 0; i < fadeInFrames; i++) {
        float ratio = static_cast<float>(i) / fadeInFrames;
        fadeInStart[i] = static_cast<int16_t>(fadeInStart[i] * ratio);
      }
    }
  }

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void playbackErrorCallback(AAudioStream *stream, void *userData,
                                  aaudio_result_t error) {
  assert(false);
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

double calcSpeed(double delta_ms) {
  const double pivot = 60.0;

  const double min_speed = 0.90;
  const double max_speed = 1.5;

  const double tau1 = 150.0;  // below pivot
  const double tau2 = 5000.0; // above pivot

  if (delta_ms < pivot) {
    double t = pivot - delta_ms;
    double factor = 1.0 - exp(-t / tau1);
    return 1.0 - (1.0 - min_speed) * factor;
  } else {
    double t = delta_ms - pivot;
    double factor = 1.0 - exp(-t / tau2);
    return 1.0 + (max_speed - 1.0) * factor;
  }
}

void audioPlaybackEnqueueFrame(std::shared_ptr<AudioPlaybackPipeline> pipeline,
                               const uint8_t *data, int32_t size) {
  if (!pipeline || pipeline->stopped.load() || size <= 0)
    return;

  std::vector<int16_t> pcm =
      audioDecoderDecode(pipeline->audioDecoder, data, size);
  if (pcm.empty())
    return;
  pipeline->playedFrames += pcm.size() / AudioPlaybackPipeline::CHANNEL_COUNT;
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
  auto const speed = calcSpeed(millis);
  FLOGI("[AUDIO] change speed from %.2f to %.2f, buffered %f ms",
        sonicGetSpeed(pipeline->sonicStream.get()), speed, millis);
  sonicSetSpeed(pipeline->sonicStream.get(), speed);
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
           samples, buffered, pipeline->currentSpeed);
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