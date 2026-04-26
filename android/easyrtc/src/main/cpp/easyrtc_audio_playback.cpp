#include "easyrtc_audio_playback.h"
#include <chrono>
#include <cstdint>
#include <cstring>
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
    // then we fill the rest of the output buffer with new PCM data from the
    // jitter buffer
    while (requestedBytes > 0 && !pipeline->jitterBuffer.empty()) {
      std::vector<uint8_t> frame;
      if (pipeline->jitterBuffer.pop(frame)) {
        int32_t toCopy =
            std::min(static_cast<int32_t>(frame.size()), requestedBytes);
        memcpy(output, frame.data(), toCopy);
        if (toCopy < static_cast<int32_t>(frame.size())) {
          pipeline->remaining_pcm_.assign(frame.begin() + toCopy, frame.end());
        }
        requestedBytes -= toCopy;
        output += toCopy;

        {
          static int64_t __idx = 0;
          if (__idx++ % 300 == 0) {
              LOGD("AUDIO PKG OUT, caches:%llu", pipeline->jitterBuffer.size());
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
  LOGD("Audio playback started: %dHz, %dch", pipeline->SAMPLE_RATE,
       pipeline->CHANNEL_COUNT);
  return 0;
}

std::shared_ptr<AudioPlaybackPipeline> audioPlaybackCreate(int audioCodec) {
  auto pipeline = std::make_shared<AudioPlaybackPipeline>();
  LOGD("AudioPlaybackPipeline created");
  pipeline->audioDecoder = audioDecoderCreate(audioCodec);
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
  if (!pipeline->playing.load() &&
      pipeline->jitterBuffer.size() >= pipeline->MIN_QUEUE_SIZE) {
    ensureStreamCreated(pipeline);
  }
  // we need to convert the int16_t PCM data to uint8_t before pushing to the
  // jitter buffer, because the buffer is defined as vector<uint8_t>
  auto frame = std::vector<uint8_t>(pcm.size() * sizeof(int16_t));
  memcpy(frame.data(), pcm.data(), frame.size());
  while (!pipeline->jitterBuffer.push(std::move(frame))) {
    // static auto __tmp = 0;
    // if (__tmp ++ % 10 == 0)
    //     LOGW("Audio playback jitter buffer full, dropping frame");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  {
    static int64_t __idx = 0;
    static int64_t frames = 0;
    frames += pcm.size() / pipeline->CHANNEL_COUNT;
    if (__idx++ % 300 == 0) {
        LOGD("AUDIO PKG IN pts:%llums, in packet caches:%llu", frames, pipeline->jitterBuffer.size());
    }
  }
}

void audioPlaybackRelease(std::shared_ptr<AudioPlaybackPipeline> pipeline) {
  if (!pipeline)
    return;

  pipeline->stopped.store(true);
  pipeline->playing.store(false);

  if (pipeline->stream) {
    AAudioStream_requestStop(pipeline->stream);
    AAudioStream_close(pipeline->stream);
    pipeline->stream = nullptr;
  }
  LOGD("AudioPlaybackPipeline released");
}
