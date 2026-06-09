#include "codec/audio_playback.h"
#include "session/media_session.h"
#include "session/common.h"
#include "utils/defer.hpp"
#include "sonic.h"
#include <aaudio/AAudio.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

#define OUT_LACK_OF_PCM_MODE_WATER_LEVEL_MS 60.0f

static int ensureStreamCreated(std::shared_ptr<AudioPlaybackPipeline> pipeline);

static aaudio_data_callback_result_t playbackCallback(AAudioStream *stream,
                                                      void *userData,
                                                      void *audioData,
                                                       int32_t numFrames) {
  auto *session = static_cast<MediaSession *>(userData);
  auto begin_ = std::chrono::steady_clock::now();
  defer({
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - begin_).count();
      if (duration > 10) {
          LOGW("[IA] callback took %lld ms", static_cast<long long>(duration));
      }
  });
  assert(session);
  if (session->shuttingDown || !session->audioPlayback || session->audioPlayback->stopped.load()) {
    LOGW("[IA] play callback with pipeline stopped or null, stopping playback");
    return AAUDIO_CALLBACK_RESULT_STOP;
  }
  auto *pipeline = session->audioPlayback.get();
  const int32_t numSamples = numFrames * pipeline->CHANNEL_COUNT;
  int32_t requestedBytes = numSamples * sizeof(int16_t);
  auto *output = static_cast<uint8_t *>(audioData);

  std::lock_guard<std::mutex> lock(pipeline->mutex_);
  int32_t avail = sonicSamplesAvailable(pipeline->sonicStream.get());
  auto millis = avail * 1000.0f /
                static_cast<float>(AudioPlaybackPipeline::SAMPLE_RATE *
                                   AudioPlaybackPipeline::CHANNEL_COUNT);
  FLOGI(
      "[IA] play callback with numFrames:%d, available samples %d, cache %f ms",
      numFrames, avail, millis);
  bool exitFromLackOfPcmState = false;
  if (pipeline->lack_of_pcm_ && millis > OUT_LACK_OF_PCM_MODE_WATER_LEVEL_MS &&
      avail > numSamples) {
    pipeline->lack_of_pcm_ = false;
    exitFromLackOfPcmState = true;
  }

  if (pipeline->lack_of_pcm_) {
    memset(output, 0, requestedBytes);
    // LOGI("[IA] in lack_of_pcm state, ignore %d request, with available:%d "
    //      "in buffer",
    //      requestedBytes, avail);
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
      LOGI("[IA] Enter lack_of_pcm state, read %d/%d "
           "samples, requested %d samples, fadeOut applied for last %d frames",
           read, avail, numSamples, fadeOutFrames);
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
      LOGW("[IA] Exit lack_of_pcm state. read %d/%d, numSamples:%d, "
           "FadeIn with %d frames",
           read, avail, numSamples, fadeInFrames);
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
  //  AAudio playback error: -899 when setSpeakerphoneOn
  if (error == AAUDIO_ERROR_DISCONNECTED) {
    LOGW("[IA] AAudio playback stream disconnected");
    auto *session = static_cast<MediaSession *>(userData);
    assert(session);
    auto pipeline = session->audioPlayback;
    if (pipeline)
      pipeline->playing.store(false);

    std::thread myThread([pipeline, stream]() {
      LOGI("AAudioStream_requestStop the stream in error callback");
      AAudioStream_requestStop(stream);
      LOGI("AAudioStream_close the stream in error callback");
      AAudioStream_close(stream);
      LOGI("AAudioStream_close done in error callback");
      if (pipeline)
        pipeline->stream = nullptr;
    });
    myThread.detach(); // Don't wait for the thread to finish.
    return;
  }
  LOGE("AAudio playback error: %d", error);
  assert(false);
}

static int ensureStreamCreated(MediaSession *session) {
  assert(session);
  if (session->shuttingDown || !session->audioPlayback ||
      session->audioPlayback->stopped.load())
    return -1;
  auto pipeline = session->audioPlayback;
  {
    std::lock_guard<std::mutex> lock(pipeline->streamMutex);
    if (pipeline->stream) {
      return 0;
    }
  }

  LOGI("[IA] AudioPlaybackOpen: %dHz %dch", AudioPlaybackPipeline::SAMPLE_RATE,
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
  AAudioStreamBuilder_setDataCallback(builder, playbackCallback, session);
  AAudioStreamBuilder_setErrorCallback(builder, playbackErrorCallback, session);

  AAudioStream *stream = nullptr;
  result = AAudioStreamBuilder_openStream(builder, &stream);
  AAudioStreamBuilder_delete(builder);

  if (result != AAUDIO_OK || !stream) {
    LOGE("Failed to open AAudio output stream: %d", result);
    return -1;
  }

  {
    std::lock_guard<std::mutex> lock(pipeline->streamMutex);
    if (pipeline->stream) {
      AAudioStream_requestStop(stream);
      AAudioStream_close(stream);
      return 0;
    }
    pipeline->stream = stream;
    pipeline->stopped.store(false);
    pipeline->lack_of_pcm_ = false;
  }

  result = AAudioStream_requestStart(stream);
  if (result != AAUDIO_OK) {
    LOGE("Failed to start AAudio output stream: %d", result);
    AAudioStream_close(stream);
    pipeline->stream = nullptr;
    return -1;
  }

  pipeline->playing.store(true);
  LOGI("[IA] AudioPlaybackOpen: SUCCESS stream=%p", stream);
  return 0;
}

std::shared_ptr<AudioPlaybackPipeline> audioPlaybackCreate(int audioCodec) {
  auto pipeline = std::make_shared<AudioPlaybackPipeline>();
  pipeline->audioDecoder = audioDecoderCreate(audioCodec);
  pipeline->sonicStream = std::shared_ptr<sonicStreamStruct>(
      sonicCreateStream(AudioPlaybackPipeline::SAMPLE_RATE,
                        AudioPlaybackPipeline::CHANNEL_COUNT),
      sonicDestroyStream);
  LOGI("[IA] AudioPlaybackCreate: sonic=%p", pipeline->sonicStream.get());
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

void audioPlaybackEnqueueFrame(MediaSession *session,
                               const EasyRTC_Frame *frame) {
  assert(session);
  auto pipeline = session->audioPlayback;
  const uint8_t *data = frame->frameData;
  int32_t size = frame->size;
  if (!pipeline || pipeline->stopped.load() || size <= 0)
    return;
  std::vector<int16_t> pcm =
      audioDecoderDecode(pipeline->audioDecoder, data, size);
  if (pcm.empty())
    return;
  const auto ptsUs = AUDIO_SAMPLES_TO_US(pipeline->playedFrames, AudioPlaybackPipeline::SAMPLE_RATE);
  pipeline->playedFrames += pcm.size() / AudioPlaybackPipeline::CHANNEL_COUNT;
  if (!pipeline->playing.load()) {
    LOGI("[IA] AudioPlaybackEnqueueFrame: not playing, trying to start stream");
    ensureStreamCreated(session);
  }

#if 0
  {
    // test sleep to simulate slow decoding and see if sonic can speed up the playback
    static int64_t _idx = 0;
    if (_idx++ % 500 == 0) {
      LOGI("[IA] Simulating slow decoding...");
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
  FLOGI("[IA] change speed from %.2f to %.2f, buffered %f ms",
        sonicGetSpeed(pipeline->sonicStream.get()), speed, millis);
  sonicSetSpeed(pipeline->sonicStream.get(), speed);
  int samples = static_cast<int>(pcm.size());
  int written =
      sonicWriteShortToStream(pipeline->sonicStream.get(), pcm.data(), samples);
  int buffered = sonicSamplesAvailable(pipeline->sonicStream.get());
  FLOGI("[IA] SonicEnqueue: wrote samples=%d buffered samples=%d "
        "speed=%.2f, sampleUs:%lld",
        samples, buffered, pipeline->currentSpeed, ptsUs);
}

void audioPlaybackRelease(MediaSession *session) {
  assert(session);
  auto pipeline = session->audioPlayback;
  if (!pipeline)
    return;

  LOGI("[IA] AudioPlaybackClose: stream=%p", pipeline->stream);

  pipeline->stopped.store(true);
  pipeline->playing.store(false);

  AAudioStream* stream = nullptr;
  {
    std::lock_guard<std::mutex> lock(pipeline->streamMutex);
    stream = pipeline->stream;
    pipeline->stream = nullptr;
  }
  if (stream) {
    AAudioStream_requestStop(stream);
    aaudio_result_t result = AAUDIO_OK;
    aaudio_stream_state_t currentState = AAudioStream_getState(stream);
    aaudio_stream_state_t inputState = currentState;
    while (result == AAUDIO_OK && currentState != AAUDIO_STREAM_STATE_STOPPED) {
        LOGI("[IA] AudioPlaybackStop: waiting for stream to stop...");
        result = AAudioStream_waitForStateChange(stream, inputState, &currentState, 100000);
        inputState = currentState;
        LOGI("[IA] AudioPlaybackStop: waiting for stream to stop: currentState=%d, we need stopped(%d)", currentState, AAUDIO_STREAM_STATE_STOPPED);
    }
    AAudioStream_close(stream);
  }
  LOGI("[IA] AudioPlaybackClose: DONE");
}

int64_t AudioPlaybackPipeline::master_clock_us() {
  assert(sonicStream);
  return AUDIO_SAMPLES_TO_US(playedFrames -
                                 sonicSamplesAvailable(sonicStream.get()),
                             AudioPlaybackPipeline::SAMPLE_RATE);
}

int64_t AudioPlaybackPipeline::cached_us() {
  assert(sonicStream);
  return sonicSamplesAvailable(sonicStream.get()) * 1000000LL /
         (SAMPLE_RATE * CHANNEL_COUNT);
}
