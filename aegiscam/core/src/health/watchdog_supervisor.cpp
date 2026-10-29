#include "aegis/health/watchdog_supervisor.h"

namespace aegis::health {

WatchdogSupervisor::WatchdogSupervisor(hal::IWatchdogHal& watchdog_hal,
                                       const common::IClock& clock)
    : watchdog_hal_(watchdog_hal), clock_(clock) {}

void WatchdogSupervisor::register_thread(std::string_view thread_name,
                                         uint64_t deadline_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  threads_[std::string(thread_name)] =
      ThreadEntry{deadline_ms, clock_.now_ms()};
}

void WatchdogSupervisor::unregister_thread(std::string_view thread_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  threads_.erase(std::string(thread_name));
}

void WatchdogSupervisor::heartbeat(std::string_view thread_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = threads_.find(std::string(thread_name));
  if (it != threads_.end()) {
    it->second.last_heartbeat_ms = clock_.now_ms();
  }
}

std::vector<std::string> WatchdogSupervisor::expired_threads() const {
  std::lock_guard<std::mutex> lock(mutex_);
  const uint64_t now = clock_.now_ms();
  std::vector<std::string> expired;
  for (const auto& [name, entry] : threads_) {
    if (now > entry.last_heartbeat_ms &&
        (now - entry.last_heartbeat_ms) > entry.deadline_ms) {
      expired.push_back(name);
    }
  }
  return expired;
}

Status WatchdogSupervisor::check_and_kick() {
  const auto expired = expired_threads();
  if (!expired.empty()) {
    std::string msg = "Stale thread heartbeat(s): ";
    for (std::size_t i = 0; i < expired.size(); ++i) {
      if (i > 0) {
        msg += ", ";
      }
      msg += expired[i];
    }
    return Status::Error(ErrorCode::kTimeout, msg);
  }
  return watchdog_hal_.kick();
}

}  // namespace aegis::health
