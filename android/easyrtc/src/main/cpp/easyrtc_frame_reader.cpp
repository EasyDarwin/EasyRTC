#include "easyrtc_frame_reader.h"
#include <cstdio>
#include <cstring>

static bool readExact(FILE* f, void* buf, size_t n) {
    return fread(buf, 1, n, f) == n;
}

bool frameReaderParse(const std::string& path, FrameFileHeader& outHeader, std::vector<FrameRecord>& outFrames) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    uint8_t headerBuf[24] = {0};
    if (!readExact(f, headerBuf, sizeof(headerBuf))) { fclose(f); return false; }

    memcpy(&outHeader.magic, headerBuf + 0, 4);
    memcpy(&outHeader.version, headerBuf + 4, 4);
    memcpy(&outHeader.videoCodec, headerBuf + 8, 4);
    memcpy(&outHeader.audioCodec, headerBuf + 12, 4);

    if (outHeader.magic != 0x44545245) { fclose(f); return false; }

    outFrames.clear();
    while (true) {
        uint8_t frameHeader[28];
        if (!readExact(f, frameHeader, sizeof(frameHeader))) break;

        FrameRecord rec;
        memcpy(&rec.callbackNs, frameHeader + 0, 8);
        memcpy(&rec.kind, frameHeader + 8, 4);
        memcpy(&rec.ptsUs, frameHeader + 12, 8);
        memcpy(&rec.size, frameHeader + 20, 4);
        memcpy(&rec.flags, frameHeader + 24, 4);

        if (rec.size > 0 && rec.size < 10 * 1024 * 1024) {
            rec.data.resize(rec.size);
            if (!readExact(f, rec.data.data(), rec.size)) break;
        }

        outFrames.push_back(std::move(rec));
    }

    fclose(f);
    return !outFrames.empty();
}
