#ifndef AEGIS_UTIL_SPSC_RING_BUFFER_H_
#define AEGIS_UTIL_SPSC_RING_BUFFER_H_

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace aegis::util {

// Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
// Capacity must be a power of 2 >= 2. Usable capacity is Capacity - 1 slots
// (or full Capacity when using monotonically increasing indices).
// Uses monotonically increasing head/tail indices with power-of-2 bitmasking,
// giving usable capacity == Capacity.
template <typename T, std::size_t Capacity>
class SpscRingBuffer {
  static_assert(Capacity >= 2 && ((Capacity & (Capacity - 1)) == 0),
                "SpscRingBuffer Capacity must be a power of 2 and >= 2");

 public:
  SpscRingBuffer() noexcept = default;
  SpscRingBuffer(const SpscRingBuffer&) = delete;
  SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

  static constexpr std::size_t capacity() noexcept { return Capacity; }

  // Producer only: attempts to push an item without blocking.
  // Returns false if the ring buffer is full.
  bool try_push(T item) noexcept(std::is_nothrow_move_assignable_v<T>) {
    const std::size_t write_idx = write_idx_.load(std::memory_order_relaxed);
    const std::size_t read_idx = read_idx_.load(std::memory_order_acquire);
    if (write_idx - read_idx >= Capacity) {
      return false;
    }
    buffer_[write_idx & kMask] = std::move(item);
    write_idx_.store(write_idx + 1, std::memory_order_release);
    return true;
  }

  // Consumer only: attempts to pop an item without blocking.
  // Returns std::nullopt if the ring buffer is empty.
  std::optional<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
    const std::size_t read_idx = read_idx_.load(std::memory_order_relaxed);
    const std::size_t write_idx = write_idx_.load(std::memory_order_acquire);
    if (read_idx == write_idx) {
      return std::nullopt;
    }
    T item = std::move(buffer_[read_idx & kMask]);
    read_idx_.store(read_idx + 1, std::memory_order_release);
    return item;
  }

  bool try_pop(T& out) noexcept(std::is_nothrow_move_assignable_v<T>) {
    const std::size_t read_idx = read_idx_.load(std::memory_order_relaxed);
    const std::size_t write_idx = write_idx_.load(std::memory_order_acquire);
    if (read_idx == write_idx) {
      return false;
    }
    out = std::move(buffer_[read_idx & kMask]);
    read_idx_.store(read_idx + 1, std::memory_order_release);
    return true;
  }

  std::size_t size() const noexcept {
    const std::size_t write_idx = write_idx_.load(std::memory_order_acquire);
    const std::size_t read_idx = read_idx_.load(std::memory_order_acquire);
    return write_idx >= read_idx ? (write_idx - read_idx) : 0;
  }

  bool empty() const noexcept { return size() == 0; }
  bool full() const noexcept { return size() >= Capacity; }

 private:
  static constexpr std::size_t kMask = Capacity - 1;

  alignas(64) std::atomic<std::size_t> write_idx_{0};
  alignas(64) std::atomic<std::size_t> read_idx_{0};
  std::array<T, Capacity> buffer_{};
};

}  // namespace aegis::util

#endif  // AEGIS_UTIL_SPSC_RING_BUFFER_H_
