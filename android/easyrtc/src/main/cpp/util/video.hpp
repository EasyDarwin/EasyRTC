
#include <cstdint>
#include <vector>



static std::string uint8_to_hex(const uint8_t* data, size_t size) {
    std::string str;
    for (size_t i = 0; i < size; i++) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", data[i]);
        str += buf;
    }
    return str;
}

static void extractSpsPpsFromAnnexb(const uint8_t* data, size_t size, std::vector<uint8_t>& sps_data,  std::vector<uint8_t>& pps_data) {
    // This is a simplified parser for annexb format, which may not cover all cases.
    // It assumes the annexb data contains one SPS and one PPS, and they are in the format of:
    // [start code][NALU header][SPS][start code][NALU header][PPS]
    // the start code could be 0x00000001 or 0x000001
    sps_data.clear();
    pps_data.clear();
    size_t pos = 0;
    while (pos + 4 < size) {
        uint64_t startCode = (data[pos] << 24) | (data[pos + 1] << 16) | (data[pos + 2] << 8) | data[pos + 3];
        bool is4Byte = (startCode == 0x00000001);
        bool is3Byte = ((startCode >> 8) == 0x000001);
        if (is4Byte || is3Byte) {
            const auto start_ = pos;
            size_t nal_start = pos + (is4Byte ? 4 : 3);
            if (nal_start >= size) break;
            uint8_t nal_type = data[nal_start] & 0x1F;
            if (nal_type == 7 || nal_type == 8) {
                // SPS or PPS
                auto &target_data = (nal_type == 7) ? sps_data : pps_data;
                size_t nal_end = nal_start + 1;
                while (nal_end + 4 < size) {
                    uint64_t nextStartCode = (data[nal_end] << 24) | (data[nal_end + 1] << 16) | (data[nal_end + 2] << 8) | data[nal_end + 3];
                    if (nextStartCode == 0x00000001 || (nextStartCode >> 8) == 0x000001) {
                        break;
                    }
                    nal_end++;
                }
                target_data.insert(target_data.end(), data + start_, data + nal_end);
                pos = nal_end;
                // auto hex = uint8_to_hex(target_data.data(), target_data.size());
                // LOGI("Extracted %s, size=%zu, data=%s", (nal_type == 7) ? "SPS" : "PPS", target_data.size(), hex.c_str());
            }else {
                break;
            }
        } else {
            pos++;
        }
    }
}