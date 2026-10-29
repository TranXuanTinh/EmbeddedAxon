#ifndef AEGIS_COMMON_CLOCK_H_
#define AEGIS_COMMON_CLOCK_H_

#include <atomic>
#include <chrono>
#include <cstdint>

namespace aegis::common {

class IClock {
 public:
  virtual ~IClock() = default;
  virtual uint64_t now_ms() const noexcept = 0;
};

class SystemClock final : public IClock {
 public:
  uint64_t now_ms() const noexcept override {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
  }
};

class FakeClock final : public IClock {
 public:
  explicit FakeClock(uint64_t initial_ms = 1700000000000ULL) noexcept
      : now_ms_(initial_ms) {}

  uint64_t now_ms() const noexcept override {
    return now_ms_.load(std::memory_order_acquire);
  }

  void advance(uint64_t delta_ms) noexcept {
    now_ms_.fetch_add(delta_ms, std::memory_order_acq_rel);
  }

  void set(uint64_t ts_ms) noexcept {
    now_ms_.store(ts_ms, std::memory_order_release);
  }

 private:
  std::atomic<uint64_t> now_ms_;
};

}  // namespace aegis::common

namespace aegis {
using common::FakeClock;
using common::IClock;
using common::SystemClock;
}  // namespace aegis

#endif  // AEGIS_COMMON_CLOCK_H_
