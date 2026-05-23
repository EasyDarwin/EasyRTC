#ifndef EASYRTC_FRAME_DUMP_H
#define EASYRTC_FRAME_DUMP_H

#include <cstdint>

struct FrameDumpWriter {
    int fd = -1;
    bool enabled = false;

    static constexpr uint32_t MAGIC = 0x44545245;
    static constexpr uint32_t VERSION = 1;
    static constexpr uint32_t KIND_VIDEO = 0;
    static constexpr uint32_t KIND_AUDIO = 1;
};

void frameDumpInit(FrameDumpWriter* writer, int videoCodec, int audioCodec);
void frameDumpWrite(FrameDumpWriter* writer, uint32_t kind, const uint8_t* data, uint32_t size, uint32_t flags);
void frameDumpClose(FrameDumpWriter* writer);

#endif
