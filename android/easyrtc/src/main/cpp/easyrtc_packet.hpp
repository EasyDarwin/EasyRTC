#ifndef EASYRTC_PACKET_H
#define EASYRTC_PACKET_H
#include <cstddef>
#include <cstdint>
#include <vector>
#include "utils/ringbuffer.hpp"

struct Packet {
  std::vector<uint8_t> data;
  int64_t ptsUs = 0;
  uint32_t frameFlags = 0;
};

class PacketRingBuffer: public easyrtc::SpscRingBuffer<Packet> {
public:
    explicit PacketRingBuffer(size_t capacity) : easyrtc::SpscRingBuffer<Packet>(capacity) {}

    uint64_t cached_millis() const {
        // we get the head and tail packet, and calculate the ptsUs delta.
        
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_acquire);
        if (head == tail) {
            return 0;
        }
        if (head > tail) {
            const Packet& headPacket = buffer_[head - 1];
            const Packet& tailPacket = buffer_[tail];
            if (headPacket.ptsUs >= tailPacket.ptsUs) {
                return static_cast<uint64_t>(headPacket.ptsUs - tailPacket.ptsUs) / 1000;
            }
        } else {
            const Packet& headPacket = buffer_[head_ - 1];
            const Packet& tailPacket = buffer_[tail];
            if (headPacket.ptsUs >= tailPacket.ptsUs) {
                return static_cast<uint64_t>(headPacket.ptsUs - tailPacket.ptsUs) / 1000;
            }
        }
        return 0;
    }
};

#endif