#ifndef AEGIS_HAL_SIM_SIM_WATCHDOG_HAL_H_
#define AEGIS_HAL_SIM_SIM_WATCHDOG_HAL_H_

#include <atomic>
#include <cstdint>

#include "aegis/hal/i_watchdog_hal.h"

namespace aegis::hal::sim {

class SimWatchdogHal final : public IWatchdogHal {
 public:
  SimWatchdogHal() noexcept = default;

  Status start(uint32_t timeout_ms) override;
  Status kick() override;
  Status stop() override;

  uint64_t kick_count() const noexcept {
    return kick_count_.load(std::memory_order_acquire);
  }
  bool is_running() const noexcept {
    return running_.load(std::memory_order_acquire);
  }
  uint32_t timeout_ms() const noexcept {
    return timeout_ms_.load(std::memory_order_acquire);
  }

 private:
  std::atomic<bool> running_{false};
  std::atomic<uint32_t> timeout_ms_{0};
  std::atomic<uint64_t> kick_count_{0};
};

}  // namespace aegis::hal::sim

#endif  // AEGIS_HAL_SIM_SIM_WATCHDOG_HAL_H_
