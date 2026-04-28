#ifndef EASYRTC_PACKET_H
#define EASYRTC_PACKET_H
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include "utils/ringbuffer.hpp"

struct Packet {
  static constexpr size_t FIXED_SIZE = 512 * 1024;
  alignas(16) uint8_t fixedBuf[FIXED_SIZE] = {};
  std::vector<uint8_t> heapBuf;
  uint32_t size = 0;
  int64_t ptsUs = 0;
  uint32_t frameFlags = 0;

  void setData(const uint8_t *src, uint32_t len) {
    size = len;
    if (len <= FIXED_SIZE) {
      if (!heapBuf.empty()) {
        heapBuf.clear();
        heapBuf.shrink_to_fit();
      }
      memcpy(fixedBuf, src, len);
    } else {
      heapBuf.assign(src, src + len);
    }
  }

  const uint8_t *data() const {
    return heapBuf.empty() ? fixedBuf : heapBuf.data();
  }

  void release() {
    if (!heapBuf.empty()) {
      heapBuf.clear();
      heapBuf.shrink_to_fit();
    }
  }
};

class PacketRingBuffer : public easyrtc::SpscRingBuffer<Packet> {
public:
  explicit PacketRingBuffer(size_t capacity) : SpscRingBuffer(capacity) {}

  uint64_t cached_millis() const {
    size_t head = head_.load(std::memory_order_relaxed);
    size_t tail = tail_.load(std::memory_order_acquire);
    if (head == tail) return 0;

    size_t newest = (head - 1) & mask_;
    const Packet &headPacket = buffer_[newest];
    const Packet &tailPacket = buffer_[tail];
    if (headPacket.ptsUs >= tailPacket.ptsUs) {
      return static_cast<uint64_t>(headPacket.ptsUs - tailPacket.ptsUs) / 1000;
    }
    return 0;
  }
};

#endif
