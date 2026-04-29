#include "easyrtc_audio_decoder.h"
#include "easyrtc_common.h"
#include "g711.h"
#include <cstring>


AudioDecoderPipeline* audioDecoderCreate(int codec) {
    auto* pipeline = new AudioDecoderPipeline();
    pipeline->codec = codec;
    LOGD("AudioDecoderPipeline created codec=%d", codec);
    return pipeline;
}

std::vector<int16_t> audioDecoderDecode(AudioDecoderPipeline* pipeline, const uint8_t* data, int32_t size) {
    std::vector<int16_t> pcm;
    if (!pipeline || !data || size <= 0) return pcm;

    if (pipeline->codec == 5) {
        pcm.resize(size);
        for (int32_t i = 0; i < size; i++) {
            pcm[i] = alaw2linear(data[i]);
        }
    } else if (pipeline->codec == 4) {
        pcm.resize(size);
        for (int32_t i = 0; i < size; i++) {
            pcm[i] = ulaw2linear(data[i]);
        }
    } else {
        LOGW("AudioDecoderPipeline: unsupported codec %d, passing through", pipeline->codec);
        pcm.assign(data, data + size);
    }

    return pcm;
}

void audioDecoderRelease(AudioDecoderPipeline* pipeline) {
    if (!pipeline) return;
    delete pipeline;
    LOGD("AudioDecoderPipeline released");
}
