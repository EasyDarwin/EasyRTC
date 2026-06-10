#ifndef EASYRTC_PACKET_H
#define EASYRTC_PACKET_H
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include "session/common.h"
#include "utils/ringbuffer.hpp"
#include "util/logger.h"

template <size_t Size>
struct PacketT {
  static constexpr size_t FIXED_SIZE = Size;
  alignas(16) uint8_t fixedBuf[FIXED_SIZE] = {};
  std::vector<uint8_t> heapBuf;
  uint32_t size = 0;
  int64_t ptsUs = 0;
  int64_t dtsUs = 0;
  uint32_t frameFlags = 0;
  uint32_t  index = 0;

  void setData(const uint8_t *src, uint32_t len) {
    size = len;
    if (len <= FIXED_SIZE && heapBuf.empty()) {
      memcpy(fixedBuf, src, len);
    } else {
      heapBuf.assign(src, src + len);
    }
  }

  const uint8_t *data() const {
    return heapBuf.empty() ? fixedBuf : heapBuf.data();
  }

  void release() {
    heapBuf.clear();
  }
};
using Packet256 = PacketT<256>;
using Packet = PacketT<4096>;
using Packet8K = PacketT<8192>;
using Packet16K = PacketT<16384>;

class PacketRingBuffer : public easyrtc::SpscRingBuffer<Packet> {
public:
  explicit PacketRingBuffer(size_t capacity) : SpscRingBuffer(capacity) {}

  uint64_t cached_us() const {
    size_t head = head_.load(std::memory_order_relaxed);
    size_t tail = tail_.load(std::memory_order_acquire);
    if (head == tail) return 0;

    size_t newest = (head - 1) & mask_;
    const Packet &headPacket = buffer_[newest];
    const Packet &tailPacket = buffer_[tail];
    if (headPacket.ptsUs >= tailPacket.ptsUs) {
      return static_cast<uint64_t>(headPacket.ptsUs - tailPacket.ptsUs);
    }
    return 0;
  }
};

#endif
