#ifndef AEGIS_HAL_I_BOOT_CONTROL_HAL_H_
#define AEGIS_HAL_I_BOOT_CONTROL_HAL_H_

#include <cstdint>
#include <string>

#include "aegis/common/result.h"

namespace aegis::hal {

struct BootSlotState {
  std::string active_slot{"A"};
  std::string pending_slot{};
  uint32_t boot_attempts_left{0};
  bool marked_good{true};
  std::string staged_sha256{};
  std::string staged_image{};
};

class IBootControlHal {
 public:
  virtual ~IBootControlHal() = default;

  virtual Result<BootSlotState> load_state() = 0;
  virtual Status save_state(const BootSlotState& state) = 0;
};

}  // namespace aegis::hal

#endif  // AEGIS_HAL_I_BOOT_CONTROL_HAL_H_
