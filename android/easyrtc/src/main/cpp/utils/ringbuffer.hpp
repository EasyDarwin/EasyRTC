#ifndef EASYRTC_RINGBUFFER_HPP
#define EASYRTC_RINGBUFFER_HPP
#include <atomic>
#include <cassert>
#include <cstddef>
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

  // Producer thread
  //   let's push be std::forwarded with move semantics, and the caller can
  //   decide whether to move or copy
  template <typename U> bool push(U &&item) {
    size_t head = head_.load(std::memory_order_relaxed);
    size_t next = (head + 1) & mask_;

    if (next == tail_.load(std::memory_order_acquire)) {
      return false; // full
    }

    buffer_[head] = std::forward<U>(item);
    head_.store(next, std::memory_order_release);
    return true;
  }

  // Consumer thread
  bool pop(T &item) {
    size_t tail = tail_.load(std::memory_order_relaxed);

    if (tail == head_.load(std::memory_order_acquire)) {
      return false; // empty
    }

    item = buffer_[tail];
    tail_.store((tail + 1) & mask_, std::memory_order_release);
    return true;
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

class vector_rb : public SpscRingBuffer<std::vector<uint8_t>> {
public:
  explicit vector_rb(size_t capacity) : SpscRingBuffer(capacity) {}
};

} // namespace easyrtc
#endif