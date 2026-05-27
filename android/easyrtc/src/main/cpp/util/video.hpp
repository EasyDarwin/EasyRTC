
#include <cstdint>
#include <vector>


#include <iostream>
#include <vector>
#include <cstdint>



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
class BitReader {
public:
    BitReader(const std::vector<uint8_t>& data)
            : data_(data), bit_pos_(0) {}

    uint32_t readBit() {
        uint32_t byte_pos = bit_pos_ / 8;
        uint32_t offset   = 7 - (bit_pos_ % 8);
        bit_pos_++;
        return (data_[byte_pos] >> offset) & 0x01;
    }

    uint32_t readBits(int n) {
        uint32_t val = 0;
        for (int i = 0; i < n; ++i) {
            val = (val << 1) | readBit();
        }
        return val;
    }

    uint32_t readUE() {
        int zeros = 0;

        while (readBit() == 0)
            zeros++;

        uint32_t value = 1;
        for (int i = 0; i < zeros; ++i) {
            value = (value << 1) | readBit();
        }

        return value - 1;
    }

    int32_t readSE() {
        uint32_t ueVal = readUE();
        int32_t val = (ueVal + 1) / 2;
        if ((ueVal & 1) == 0)
            val = -val;
        return val;
    }

private:
    const std::vector<uint8_t>& data_;
    size_t bit_pos_;
};

static std::vector<uint8_t> removeEmulationBytes(
        const uint8_t* data,
        size_t size)
{
    std::vector<uint8_t> out;

    for (size_t i = 0; i < size; ++i) {
        if (i + 2 < size &&
            data[i] == 0x00 &&
            data[i + 1] == 0x00 &&
            data[i + 2] == 0x03)
        {
            out.push_back(0x00);
            out.push_back(0x00);
            i += 2;
            continue;
        }

        out.push_back(data[i]);
    }

    return out;
}

bool parseSPS(
        const uint8_t* sps,
        size_t size,
        int& width,
        int& height)
{
    size_t offset = 0;
    if (size >= 4 && sps[0] == 0x00 && sps[1] == 0x00 && sps[2] == 0x00 && sps[3] == 0x01)
        offset = 4;
    else if (size >= 3 && sps[0] == 0x00 && sps[1] == 0x00 && sps[2] == 0x01)
        offset = 3;
    offset += 1; // skip NAL header byte
    if (offset >= size) return false;
    std::vector<uint8_t> rbsp =
            removeEmulationBytes(sps + offset, size - offset);

    BitReader br(rbsp);

    uint8_t profile_idc = br.readBits(8);
    br.readBits(8); // constraint flags
    br.readBits(8); // level_idc
    br.readUE();    // seq_parameter_set_id

    // high profile stuff
    if (profile_idc == 100 ||
        profile_idc == 110 ||
        profile_idc == 122 ||
        profile_idc == 244 ||
        profile_idc == 44  ||
        profile_idc == 83  ||
        profile_idc == 86)
    {
        uint32_t chroma_format_idc = br.readUE();

        if (chroma_format_idc == 3)
            br.readBit(); // separate_colour_plane_flag

        br.readUE(); // bit_depth_luma_minus8
        br.readUE(); // bit_depth_chroma_minus8
        br.readBit(); // qpprime_y_zero_transform_bypass_flag

        uint32_t seq_scaling_matrix_present_flag = br.readBit();

        if (seq_scaling_matrix_present_flag) {
            // skip scaling lists (not needed here)
        }
    }

    br.readUE(); // log2_max_frame_num_minus4

    uint32_t pic_order_cnt_type = br.readUE();

    if (pic_order_cnt_type == 0) {
        br.readUE();
    } else if (pic_order_cnt_type == 1) {
        br.readBit();
        br.readSE();
        br.readSE();

        uint32_t num_ref_frames_in_pic_order_cnt_cycle =
                br.readUE();

        for (uint32_t i = 0;
             i < num_ref_frames_in_pic_order_cnt_cycle;
             i++)
        {
            br.readSE();
        }
    }

    br.readUE(); // max_num_ref_frames
    br.readBit(); // gaps_in_frame_num_value_allowed_flag

    uint32_t pic_width_in_mbs_minus1  = br.readUE();
    uint32_t pic_height_in_map_units_minus1 = br.readUE();

    uint32_t frame_mbs_only_flag = br.readBit();

    if (!frame_mbs_only_flag) {
        br.readBit();
    }

    br.readBit(); // direct_8x8_inference_flag

    uint32_t frame_cropping_flag = br.readBit();

    uint32_t crop_left = 0;
    uint32_t crop_right = 0;
    uint32_t crop_top = 0;
    uint32_t crop_bottom = 0;

    if (frame_cropping_flag) {
        crop_left   = br.readUE();
        crop_right  = br.readUE();
        crop_top    = br.readUE();
        crop_bottom = br.readUE();
    }

    width  = (pic_width_in_mbs_minus1 + 1) * 16;
    height = (pic_height_in_map_units_minus1 + 1) * 16;

    if (!frame_mbs_only_flag) {
        height *= 2;
    }

    // assume 4:2:0
    width  -= (crop_left + crop_right) * 2;
    height -= (crop_top + crop_bottom) * 2;

    return true;
}