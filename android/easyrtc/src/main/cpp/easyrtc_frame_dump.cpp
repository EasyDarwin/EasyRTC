#include "easyrtc_frame_dump.h"
#include "easyrtc_common.h"
#include <sys/system_properties.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <cstring>
#include <string>
#if 0
void frameDumpInit(FrameDumpWriter* writer, int videoCodec, int audioCodec) {
    char cmdline[256] = {0};
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd < 0) { writer->enabled = false; return; }
    read(fd, cmdline, sizeof(cmdline) - 1);
    close(fd);
    if (cmdline[0] == '\0') { writer->enabled = false; return; }

    std::string dumpDir = std::string("/sdcard/Android/data/") + cmdline + "/files";
    mkdir(dumpDir.c_str(), 0777);
    std::string dumpPath = dumpDir + "/easyrtc_av.bin";
    writer->fd = open(dumpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (writer->fd < 0) {
        LOGE("frameDumpInit: failed to open %s", dumpPath.c_str());
        writer->enabled = false;
        return;
    }

    uint8_t header[24] = {0};
    uint32_t magic = FrameDumpWriter::MAGIC;
    uint32_t version = FrameDumpWriter::VERSION;
    memcpy(header + 0, &magic, 4);
    memcpy(header + 4, &version, 4);
    memcpy(header + 8, &videoCodec, 4);
    memcpy(header + 12, &audioCodec, 4);

    write(writer->fd, header, sizeof(header));
    writer->enabled = true;
    LOGD("frameDumpInit: dumping to %s videoCodec=%d audioCodec=%d", dumpPath.c_str(), videoCodec, audioCodec);
}

void frameDumpWrite(FrameDumpWriter* writer, uint32_t kind, const uint8_t* data, uint32_t size, int64_t ptsUs, uint32_t flags) {
    if (!writer->enabled || writer->fd < 0) return;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    int64_t callbackNs = static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;

    uint8_t frameHeader[28];
    memcpy(frameHeader + 0, &callbackNs, 8);
    memcpy(frameHeader + 8, &kind, 4);
    memcpy(frameHeader + 12, &ptsUs, 8);
    memcpy(frameHeader + 20, &size, 4);
    memcpy(frameHeader + 24, &flags, 4);

    write(writer->fd, frameHeader, sizeof(frameHeader));
    write(writer->fd, data, size);
}

void frameDumpClose(FrameDumpWriter* writer) {
    if (writer->fd >= 0) {
        close(writer->fd);
        writer->fd = -1;
    }
    writer->enabled = false;
}
#else
void frameDumpInit(FrameDumpWriter* writer, int videoCodec, int audioCodec) {

}
void frameDumpWrite(FrameDumpWriter* writer, uint32_t kind, const uint8_t* data, uint32_t size, uint32_t flags) {

}
void frameDumpClose(FrameDumpWriter* writer) {

}
#endif