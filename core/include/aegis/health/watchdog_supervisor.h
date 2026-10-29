#ifndef AEGIS_HEALTH_WATCHDOG_SUPERVISOR_H_
#define AEGIS_HEALTH_WATCHDOG_SUPERVISOR_H_

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aegis/common/clock.h"
#include "aegis/common/result.h"
#include "aegis/hal/i_watchdog_hal.h"

namespace aegis::health {

class WatchdogSupervisor {
 public:
  WatchdogSupervisor(hal::IWatchdogHal& watchdog_hal,
                     const common::IClock& clock);

  // Registers a monitored worker thread with its maximum allowed heartbeat
  // interval (deadline_ms).
  void register_thread(std::string_view thread_name, uint64_t deadline_ms);

  void unregister_thread(std::string_view thread_name);

  // Records a fresh heartbeat for `thread_name` at `clock.now_ms()`.
  void heartbeat(std::string_view thread_name);

  // Evaluates all registered threads. If all heartbeats are within their
  // deadline, kicks the hardware watchdog and returns Ok. Otherwise does NOT
  // kick the hardware watchdog and returns an error listing expired threads.
  Status check_and_kick();

  std::vector<std::string> expired_threads() const;

 private:
  struct ThreadEntry {
    uint64_t deadline_ms{0};
    uint64_t last_heartbeat_ms{0};
  };

  hal::IWatchdogHal& watchdog_hal_;
  const common::IClock& clock_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, ThreadEntry> threads_;
};

}  // namespace aegis::health

#endif  // AEGIS_HEALTH_WATCHDOG_SUPERVISOR_H_
