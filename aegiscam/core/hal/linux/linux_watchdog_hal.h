#ifndef AEGIS_HAL_LINUX_LINUX_WATCHDOG_HAL_H_
#define AEGIS_HAL_LINUX_LINUX_WATCHDOG_HAL_H_

#include <cstdint>
#include <string>

#include "aegis/hal/i_watchdog_hal.h"

namespace aegis::hal::linux_hal {

// Linux /dev/watchdog HAL using WDIOC_SETTIMEOUT / WDIOC_KEEPALIVE and magic
// close character 'V' per Linux watchdog kernel API.
class LinuxWatchdogHal final : public IWatchdogHal {
 public:
  explicit LinuxWatchdogHal(std::string dev_path = "/dev/watchdog");
  ~LinuxWatchdogHal() override;

  LinuxWatchdogHal(const LinuxWatchdogHal&) = delete;
  LinuxWatchdogHal& operator=(const LinuxWatchdogHal&) = delete;

  Status start(uint32_t timeout_ms) override;
  Status kick() override;
  Status stop() override;

 private:
  std::string dev_path_;
  int fd_{-1};
};

}  // namespace aegis::hal::linux_hal

#endif  // AEGIS_HAL_LINUX_LINUX_WATCHDOG_HAL_H_
