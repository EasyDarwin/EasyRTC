#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <cassert>
#include <fstream>
#include <iostream>

#include "../easyrtc/src/main/cpp/util/video.hpp"
#include "../easyrtc/src/main/cpp/util/frame_reader.h"

#define LOG(fmt, ...) do { printf(fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

static std::string hex(const uint8_t* data, size_t size) {
    return uint8_to_hex(data, size);
}

static std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", path.c_str());
        exit(1);
    }
    return {std::istreambuf_iterator<char>(f), {}};
}

static void testExtractSpsPpsFromAnnexb() {
    LOG("=== testExtractSpsPpsFromAnnexb ===");
    auto data = readFile("easyrtc_key_frame.bin");
    LOG("File size: %zu bytes", data.size());
    LOG("First 32 bytes: %s", hex(data.data(), std::min(size_t(32), data.size())).c_str());

    std::vector<uint8_t> sps, pps;
    LOG("Calling extractSpsPpsFromAnnexb...");
    extractSpsPpsFromAnnexb(data.data(), data.size(), sps, pps);

    LOG("SPS extracted: %zu bytes, hex=%s", sps.size(), hex(sps.data(), sps.size()).c_str());
    LOG("PPS extracted: %zu bytes, hex=%s", pps.size(), hex(pps.data(), pps.size()).c_str());

    assert(!sps.empty() && "SPS should not be empty");
    assert(!pps.empty() && "PPS should not be empty");

    assert(sps.size() >= 5);
    size_t sps_nal_offset = (sps[0] == 0x00 && sps[1] == 0x00 && sps[2] == 0x00 && sps[3] == 0x01) ? 4 : 3;
    LOG("SPS NAL byte at offset %zu: 0x%02x, type=%d", sps_nal_offset, sps[sps_nal_offset], sps[sps_nal_offset] & 0x1F);
    assert((sps[sps_nal_offset] & 0x1F) == 7 && "SPS NAL type should be 7");

    assert(pps.size() >= 5);
    size_t pps_nal_offset = (pps[0] == 0x00 && pps[1] == 0x00 && pps[2] == 0x00 && pps[3] == 0x01) ? 4 : 3;
    LOG("PPS NAL byte at offset %zu: 0x%02x, type=%d", pps_nal_offset, pps[pps_nal_offset], pps[pps_nal_offset] & 0x1F);
    assert((pps[pps_nal_offset] & 0x1F) == 8 && "PPS NAL type should be 8");

    LOG("PASS");
    LOG("");
}

static void testParseSPS() {
    LOG("=== testParseSPS ===");
    auto data = readFile("easyrtc_key_frame.bin");

    std::vector<uint8_t> sps, pps;
    extractSpsPpsFromAnnexb(data.data(), data.size(), sps, pps);

    LOG("SPS passed to parseSPS: %zu bytes, hex=%s", sps.size(), hex(sps.data(), sps.size()).c_str());
    LOG("SPS first byte: 0x%02x", sps[0]);
    LOG("parseSPS does: removeEmulationBytes(sps+1, size-1)");
    LOG("  -> skips only 1 byte, starts at: 0x%02x", sps[1]);

    int width = 0, height = 0;
    LOG("Calling parseSPS...");
    bool ok = parseSPS(sps.data(), sps.size(), width, height);

    LOG("parseSPS result: ok=%d, width=%d, height=%d", ok, width, height);

    assert(ok && "parseSPS should succeed");
    assert(width > 0 && height > 0 && "Dimensions should be positive");
    LOG("Resolution: %dx%d", width, height);

    LOG("PASS");
    LOG("");
}

static void testParseSPSFixed() {
    LOG("=== testParseSPS (fixed: strip start code first) ===");
    auto data = readFile("easyrtc_key_frame.bin");

    std::vector<uint8_t> sps, pps;
    extractSpsPpsFromAnnexb(data.data(), data.size(), sps, pps);

    size_t nal_offset = (sps[0] == 0x00 && sps[1] == 0x00 && sps[2] == 0x00 && sps[3] == 0x01) ? 4 : 3;
    LOG("SPS NAL starts at offset %zu, NAL header byte: 0x%02x", nal_offset, sps[nal_offset]);

    std::vector<uint8_t> sps_nal(sps.begin() + nal_offset, sps.end());
    LOG("SPS NAL body (with header): %zu bytes, hex=%s", sps_nal.size(), hex(sps_nal.data(), sps_nal.size()).c_str());

    int width = 0, height = 0;
    LOG("Calling parseSPS with NAL body...");
    bool ok = parseSPS(sps_nal.data(), sps_nal.size(), width, height);

    LOG("parseSPS result: ok=%d, width=%d, height=%d", ok, width, height);

    if (ok && width > 0 && height > 0) {
        LOG("Resolution: %dx%d", width, height);
        LOG("PASS");
    } else {
        LOG("FAILED - parseSPS returned invalid result");
    }
    LOG("");
}

static void testBitReader() {
    LOG("=== testBitReader ===");
    std::vector<uint8_t> v = {0xA5};
    BitReader br(v);
    assert(br.readBit() == 1);
    assert(br.readBit() == 0);
    assert(br.readBit() == 1);
    assert(br.readBit() == 0);
    assert(br.readBit() == 0);
    assert(br.readBit() == 1);
    assert(br.readBit() == 0);
    assert(br.readBit() == 1);
    LOG("readBit: PASS");

    std::vector<uint8_t> v2 = {0xFF, 0x00};
    BitReader br2(v2);
    assert(br2.readBits(8) == 0xFF);
    assert(br2.readBits(8) == 0x00);
    LOG("readBits: PASS");

    // readUE: encoded as Exp-Golomb
    // 0 -> 1 (1 bit)
    // 1 -> 010 (3 bits)
    // 2 -> 011 (3 bits)
    // 3 -> 00100 (5 bits)
    // Binary: 1 010 011 00100 = 0b10100110 0100xxxx
    std::vector<uint8_t> v3 = {0xA6, 0x40};
    BitReader br3(v3);
    LOG("readUE test:");
    auto val0 = br3.readUE(); LOG("  readUE() = %u (expect 0)", val0);
    auto val1 = br3.readUE(); LOG("  readUE() = %u (expect 1)", val1);
    auto val2 = br3.readUE(); LOG("  readUE() = %u (expect 2)", val2);
    auto val3 = br3.readUE(); LOG("  readUE() = %u (expect 3)", val3);
    assert(val0 == 0);
    assert(val1 == 1);
    assert(val2 == 2);
    assert(val3 == 3);
    LOG("readUE: PASS");

    LOG("PASS");
    LOG("");
}

static void testRemoveEmulationBytes() {
    LOG("=== testRemoveEmulationBytes ===");
    uint8_t input[] = {0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x01, 0x02};
    auto result = removeEmulationBytes(input, sizeof(input));
    LOG("Input:  %s", hex(input, sizeof(input)).c_str());
    LOG("Output: %s", hex(result.data(), result.size()).c_str());
    assert(result.size() == sizeof(input) - 2);
    assert(result[0] == 0x00 && result[1] == 0x00);
    assert(result[2] == 0x00 && result[3] == 0x00);
    LOG("PASS");
    LOG("");
}

static void testUint8ToHex() {
    LOG("=== testUint8ToHex ===");
    uint8_t data[] = {0x00, 0x01, 0xAB, 0xFF};
    auto result = uint8_to_hex(data, sizeof(data));
    assert(result == "0001abff");
    LOG("uint8_to_hex({0x00,0x01,0xAB,0xFF}) = %s", result.c_str());
    LOG("PASS");
    LOG("");
}

static void testHEVCExtractOnAVCData() {
    LOG("=== testHEVCExtractOnAVCData (should yield empty) ===");
    auto data = readFile("easyrtc_key_frame.bin");
    std::vector<uint8_t> csd0;
    extractH265VpsSpsPpsFromAnnexb(data.data(), data.size(), csd0);
    LOG("HEVC extract on AVC data: csd0 size=%zu (expect 0)", csd0.size());
    assert(csd0.empty() && "AVC data should not produce HEVC csd0");
    LOG("PASS");
    LOG("");
}

static void testHEVCExtractReal() {
    LOG("=== testHEVCExtractReal ===");
    auto data = readFile("easyrtc_key_frame_hevc.bin");
    LOG("HEVC file size: %zu bytes", data.size());

    std::vector<uint8_t> csd0;
    extractH265VpsSpsPpsFromAnnexb(data.data(), data.size(), csd0);
    LOG("HEVC csd0 size: %zu", csd0.size());
    assert(!csd0.empty() && "HEVC csd0 should not be empty");
    LOG("HEVC csd0: %s", hex(csd0.data(), csd0.size()).c_str());

    // Verify first NAL is VPS (type 32)
    assert(csd0.size() >= 5);
    bool sc4 = (csd0[0] == 0x00 && csd0[1] == 0x00 && csd0[2] == 0x00 && csd0[3] == 0x01);
    size_t off0 = sc4 ? 4 : 3;
    uint8_t nalType0 = (csd0[off0] >> 1) & 0x3F;
    LOG("First NAL type: %d (expect 32=VPS)", nalType0);
    assert(nalType0 == 32);

    // Verify csd0 does NOT contain IDR (type 19 or 20)
    bool hasIDR = false;
    for (size_t i = 0; i + 3 < csd0.size();) {
        if (csd0[i] == 0x00 && csd0[i+1] == 0x00 && csd0[i+2] == 0x01) {
            uint8_t nt = (csd0[i+3] >> 1) & 0x3F;
            if (nt == 19 || nt == 20) { hasIDR = true; break; }
            i += 4;
        } else {
            i++;
        }
    }
    assert(!hasIDR && "csd0 should not contain IDR");
    LOG("No IDR in csd0: OK");
    LOG("PASS");
    LOG("");
}

int main(int argc, char** argv) {
    LOG("Testing util/video.hpp with easyrtc_key_frame.bin");
    LOG("================================================");
    LOG("");

    testUint8ToHex();
    testBitReader();
    testRemoveEmulationBytes();
    testExtractSpsPpsFromAnnexb();
    testParseSPS();
    testParseSPSFixed();
    testHEVCExtractOnAVCData();
    testHEVCExtractReal();

    LOG("Done.");
    return 0;
}
