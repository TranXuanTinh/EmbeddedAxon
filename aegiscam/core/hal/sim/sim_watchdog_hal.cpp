#include "hal/sim/sim_watchdog_hal.h"

namespace aegis::hal::sim {

Status SimWatchdogHal::start(uint32_t timeout_ms) {
  if (timeout_ms == 0) {
    return Status::Error(ErrorCode::kInvalidArgument,
                         "Watchdog timeout must be > 0");
  }
  timeout_ms_.store(timeout_ms, std::memory_order_release);
  running_.store(true, std::memory_order_release);
  return Status::Ok();
}

Status SimWatchdogHal::kick() {
  if (!running_.load(std::memory_order_acquire)) {
    return Status::Error(ErrorCode::kInvalidState, "Watchdog is not started");
  }
  kick_count_.fetch_add(1, std::memory_order_acq_rel);
  return Status::Ok();
}

Status SimWatchdogHal::stop() {
  running_.store(false, std::memory_order_release);
  return Status::Ok();
}

}  // namespace aegis::hal::sim
