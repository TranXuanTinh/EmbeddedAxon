#ifndef AEGIS_HAL_SIM_SIM_BOOT_CONTROL_HAL_H_
#define AEGIS_HAL_SIM_SIM_BOOT_CONTROL_HAL_H_

#include <string>
#include <string_view>

#include "aegis/hal/i_boot_control_hal.h"
#include "aegis/hal/i_storage_hal.h"

namespace aegis::hal::sim {

class SimBootControlHal final : public IBootControlHal {
 public:
  static constexpr std::string_view kDefaultRelPath = "ota/slot_state.json";

  explicit SimBootControlHal(
      IStorageHal& storage,
      std::string rel_path = std::string(kDefaultRelPath));

  Result<BootSlotState> load_state() override;
  Status save_state(const BootSlotState& state) override;

  static std::string serialize_state(const BootSlotState& state);
  static Result<BootSlotState> parse_state(std::string_view json);

 private:
  IStorageHal& storage_;
  std::string rel_path_;
};

}  // namespace aegis::hal::sim

#endif  // AEGIS_HAL_SIM_SIM_BOOT_CONTROL_HAL_H_
