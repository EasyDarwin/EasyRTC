#include "easyrtc_audio_playback.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#include <unistd.h>

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
  // first we consume the remaining PCM data from the previous callback if any
  if (!pipeline->remaining_pcm_.empty()) {
//    LOGW("remaining pcm cached in pipeline, bytes:%llu",
//         pipeline->remaining_pcm_.size());
    int32_t toCopy = std::min(
        static_cast<int32_t>(pipeline->remaining_pcm_.size()), requestedBytes);
    memcpy(output, pipeline->remaining_pcm_.data(), toCopy);
    if (toCopy < static_cast<int32_t>(pipeline->remaining_pcm_.size())) {
      pipeline->remaining_pcm_.erase(pipeline->remaining_pcm_.begin(),
                                     pipeline->remaining_pcm_.begin() + toCopy);
    } else {
      pipeline->remaining_pcm_.clear();
    }
    requestedBytes -= toCopy;
    output += toCopy;
  }
  if (pipeline->lack_of_pcm_ &&
      pipeline->jitterBuffer.size() < pipeline->MIN_QUEUE_SIZE) {
    memset(output, 0, requestedBytes);
    requestedBytes = 0;
  } else {
      if (pipeline->lack_of_pcm_) {
          LOGW("pcm data is back from insufficient");
          pipeline->lack_of_pcm_ = false;
      }

    size_t queueSize = pipeline->jitterBuffer.size();
    bool speedUp = queueSize > pipeline->SPEED_UP_THRESHOLD && pipeline->sonicStream;

    if (speedUp) {
      float speed = 1.0f + static_cast<float>(queueSize - pipeline->SPEED_UP_THRESHOLD) /
                             static_cast<float>(pipeline->MAX_QUEUE_SIZE - pipeline->SPEED_UP_THRESHOLD);
      speed = speed > 2.0f ? 2.0f : speed;
      sonicSetSpeed(pipeline->sonicStream.get(), speed);

      int32_t totalInputFrames = requestedBytes / sizeof(int16_t);
      int32_t collectedBytes = 0;

      auto *pcmBuf = static_cast<int16_t*>(static_cast<void*>(output));

      while (collectedBytes < requestedBytes * 2 && !pipeline->jitterBuffer.empty()) {
        easyrtc::AudioSlot *slot = pipeline->jitterBuffer.acquirePop();
        if (!slot) break;
        int32_t avail = static_cast<int32_t>(slot->size);
        int32_t samples = avail / static_cast<int32_t>(sizeof(int16_t));
        sonicWriteShortToStream(pipeline->sonicStream.get(),
                                reinterpret_cast<const int16_t*>(slot->data()), samples);
        pipeline->jitterBuffer.commitPop();
        collectedBytes += avail;
      }

      int availableSamples = sonicSamplesAvailable(pipeline->sonicStream.get());
      int requestedSamples = requestedBytes / static_cast<int32_t>(sizeof(int16_t));
      if (availableSamples > 0) {
        int toRead = availableSamples > requestedSamples ? requestedSamples : availableSamples;
        sonicReadShortFromStream(pipeline->sonicStream.get(), pcmBuf, toRead);
        requestedBytes -= toRead * static_cast<int32_t>(sizeof(int16_t));
        output += toRead * static_cast<int32_t>(sizeof(int16_t));
      }

      {
        static int64_t __idx = 0;
        if (__idx++ % 100 == 0) {
            LOGW("AUDIO SPEEDUP speed=%.2f queue=%zu", speed, pipeline->jitterBuffer.size());
        }
      }
    } else {
      if (pipeline->sonicStream) {
        sonicSetSpeed(pipeline->sonicStream.get(), 1.0f);
      }
    while (requestedBytes > 0 && !pipeline->jitterBuffer.empty()) {
      easyrtc::AudioSlot *slot = pipeline->jitterBuffer.acquirePop();
      if (!slot) break;
      int32_t toCopy =
          std::min(static_cast<int32_t>(slot->size), requestedBytes);
      memcpy(output, slot->data(), toCopy);
      if (toCopy < static_cast<int32_t>(slot->size)) {
        pipeline->remaining_pcm_.assign(slot->data() + toCopy, slot->data() + slot->size);
      }
      pipeline->jitterBuffer.commitPop();
      requestedBytes -= toCopy;
      output += toCopy;

      {
        static int64_t __idx = 0;
        if (__idx++ % 300 == 0) {
            LOGD("AUDIO OUT, PKT cached:%llu", pipeline->jitterBuffer.size());
        }
      }
    }
    }
    assert(requestedBytes >= 0);
    if (requestedBytes > 0) {
      LOGW("insufficient audio data in jitter buffer, filling silence for %d "
           "bytes",
           requestedBytes);
      memset(output, 0, requestedBytes);
      pipeline->lack_of_pcm_ = true;
    }
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

  LOGI("[CRITICAL] AudioPlaybackOpen: %dHz %dch", pipeline->SAMPLE_RATE, pipeline->CHANNEL_COUNT);

  AAudioStreamBuilder *builder = nullptr;
  aaudio_result_t result = AAudio_createStreamBuilder(&builder);
  if (result != AAUDIO_OK || !builder) {
    LOGE("Failed to create AAudio stream builder: %d", result);
    return -1;
  }

  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setSampleRate(builder, pipeline->SAMPLE_RATE);
  AAudioStreamBuilder_setChannelCount(builder, pipeline->CHANNEL_COUNT);
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
  LOGI("[CRITICAL] AudioPlaybackOpen: SUCCESS stream=%p %dHz %dch",
       stream, pipeline->SAMPLE_RATE, pipeline->CHANNEL_COUNT);
  return 0;
}

std::shared_ptr<AudioPlaybackPipeline> audioPlaybackCreate(int audioCodec) {
  auto pipeline = std::make_shared<AudioPlaybackPipeline>();
  LOGD("AudioPlaybackPipeline created");
  pipeline->audioDecoder = audioDecoderCreate(audioCodec);
   pipeline->sonicStream = std::shared_ptr<sonicStreamStruct>(sonicCreateStream(pipeline->SAMPLE_RATE, pipeline->CHANNEL_COUNT), sonicDestroyStream);
   LOGD("AudioPlaybackPipeline created sonic=%p", pipeline->sonicStream.get());
   return pipeline;

  return pipeline;
}

void audioPlaybackEnqueueFrame(std::shared_ptr<AudioPlaybackPipeline> pipeline,
                               const uint8_t *data, int32_t size) {
  if (!pipeline || pipeline->stopped.load() || size <= 0)
    return;

  std::vector<int16_t> pcm =
      audioDecoderDecode(pipeline->audioDecoder, data, size);
  if (pcm.empty()) {
    return;
  }
  if (!pipeline->playing.load()) {
    ensureStreamCreated(pipeline);
  }
  auto pcmBytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
  auto *pcmData = reinterpret_cast<const uint8_t *>(pcm.data());
  easyrtc::AudioSlot *slot = pipeline->jitterBuffer.acquirePush();
  if (!slot) {
    do {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      slot = pipeline->jitterBuffer.acquirePush();
    } while (!slot);
  }
  slot->setData(pcmData, pcmBytes);
  pipeline->jitterBuffer.commitPush();
  {
    static int64_t __idx = 0;
    static int64_t frames = 0;
    frames += pcm.size() / pipeline->CHANNEL_COUNT;
    if (__idx++ % 300 == 0) {
        LOGD("AUDIO PKG IN pts:%llums, in PKT cached:%llu", frames, pipeline->jitterBuffer.size());
    }
  }
}

void audioPlaybackRelease(std::shared_ptr<AudioPlaybackPipeline> pipeline) {
  if (!pipeline)
    return;

  LOGI("[CRITICAL] AudioPlaybackClose: stream=%p", pipeline->stream);

  pipeline->stopped.store(true);
  pipeline->playing.store(false);

  if (pipeline->stream) {
    AAudioStream_requestStop(pipeline->stream);
    AAudioStream_close(pipeline->stream);
    pipeline->stream = nullptr;
  }
  LOGI("[CRITICAL] AudioPlaybackClose: DONE");
}
