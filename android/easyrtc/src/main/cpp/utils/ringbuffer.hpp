#ifndef EASYRTC_RINGBUFFER_HPP
#define EASYRTC_RINGBUFFER_HPP
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <vector>
namespace easyrtc {

template <typename T> class SpscRingBuffer {
public:
  explicit SpscRingBuffer(size_t capacity)
      : capacity_(round_up_pow2(capacity)), mask_(capacity_ - 1),
        buffer_(capacity_) {
    assert(capacity_ >= 2);
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
  }

  // Zero-copy push: acquire slot, write into it, then commit
  T *acquirePush() {
    size_t head = head_.load(std::memory_order_relaxed);
    size_t next = (head + 1) & mask_;
    if (next == tail_.load(std::memory_order_acquire)) {
      return nullptr;
    }
    return &buffer_[head];
  }

  void commitPush() {
    size_t head = head_.load(std::memory_order_relaxed);
    head_.store((head + 1) & mask_, std::memory_order_release);
  }

  // Zero-copy pop: acquire slot, read from it, then release
  T *acquirePop() {
    size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return nullptr;
    }
    return &buffer_[tail];
  }

  void commitPop() {
    size_t tail = tail_.load(std::memory_order_relaxed);
    auto &slot = buffer_[tail];
    slot.release();
    tail_.store((tail + 1) & mask_, std::memory_order_release);
  }

  bool empty() const {
    return head_.load(std::memory_order_acquire) ==
           tail_.load(std::memory_order_acquire);
  }

  bool full() const {
    size_t next = (head_.load(std::memory_order_acquire) + 1) & mask_;
    return next == tail_.load(std::memory_order_acquire);
  }

  size_t size() const {
    size_t head = head_.load(std::memory_order_acquire);
    size_t tail = tail_.load(std::memory_order_acquire);
    if (head >= tail) {
      return head - tail;
    } else {
      return capacity_ - (tail - head);
    }
  }

private:
  static size_t round_up_pow2(size_t n) {
    size_t p = 1;
    while (p < n)
      p <<= 1;
    return p;
  }

protected:
  const size_t capacity_;
  const size_t mask_;
  std::vector<T> buffer_;

  alignas(64) std::atomic<size_t> head_;
  alignas(64) std::atomic<size_t> tail_;
};

struct AudioSlot {
  static constexpr size_t FIXED_SIZE = 1024;
  alignas(16) uint8_t fixedBuf[FIXED_SIZE] = {};
  std::vector<uint8_t> heapBuf;
  uint32_t size = 0;

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

class vector_rb : public SpscRingBuffer<AudioSlot> {
public:
  explicit vector_rb(size_t capacity) : SpscRingBuffer(capacity) {}
};

} // namespace easyrtc
#endif
