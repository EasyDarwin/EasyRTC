#ifndef EASYRTC_AUDIO_DECODER_H
#define EASYRTC_AUDIO_DECODER_H

#include <cstdint>
#include <vector>

struct AudioDecoderPipeline {
    int codec = 0;
};

AudioDecoderPipeline* audioDecoderCreate(int codec);
std::vector<uint8_t> audioDecoderDecode(AudioDecoderPipeline* pipeline, const uint8_t* data, int32_t size);
void audioDecoderRelease(AudioDecoderPipeline* pipeline);

#endif
