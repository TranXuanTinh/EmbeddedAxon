#ifndef AEGIS_HAL_I_WATCHDOG_HAL_H_
#define AEGIS_HAL_I_WATCHDOG_HAL_H_

#include <cstdint>

#include "aegis/common/result.h"

namespace aegis::hal {

class IWatchdogHal {
 public:
  virtual ~IWatchdogHal() = default;

  virtual Status start(uint32_t timeout_ms) = 0;
  virtual Status kick() = 0;
  virtual Status stop() = 0;
};

}  // namespace aegis::hal

#endif  // AEGIS_HAL_I_WATCHDOG_HAL_H_
