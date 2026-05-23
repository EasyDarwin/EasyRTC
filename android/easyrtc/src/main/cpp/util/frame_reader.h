#ifndef EASYRTC_FRAME_READER_H
#define EASYRTC_FRAME_READER_H

#include <cstdint>
#include <vector>
#include <string>

struct FrameRecord {
    int64_t callbackNs = 0;
    uint32_t kind = 0;
    int64_t ptsUs = 0;
    uint32_t size = 0;
    uint32_t flags = 0;
    std::vector<uint8_t> data;
};

struct FrameFileHeader {
    uint32_t magic = 0;
    uint32_t version = 0;
    int32_t videoCodec = 0;
    int32_t audioCodec = 0;
};

bool frameReaderParse(const std::string& path, FrameFileHeader& outHeader, std::vector<FrameRecord>& outFrames);

#endif
