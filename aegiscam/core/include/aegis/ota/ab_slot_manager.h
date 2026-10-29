#ifndef AEGIS_OTA_AB_SLOT_MANAGER_H_
#define AEGIS_OTA_AB_SLOT_MANAGER_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "aegis/common/result.h"
#include "aegis/hal/i_boot_control_hal.h"
#include "aegis/hal/i_storage_hal.h"

namespace aegis::ota {

class AbSlotManager {
 public:
  static constexpr uint32_t kDefaultBootAttempts = 3;

  AbSlotManager(hal::IBootControlHal& boot_control, hal::IStorageHal& storage);

  // Loads slot state from BootControlHal, or initializes default Slot A.
  Status init();

  // Verifies image at `image_path` matches `expected_sha256_hex`, then stages
  // the inactive slot as pending with `boot_attempts_left = 3`.
  Status stage(std::string_view image_path,
               std::string_view expected_sha256_hex);

  // Simulates / handles a bootloader reboot cycle:
  // - If a pending_slot exists and active_slot != pending_slot (first boot into
  //   new image), switches active_slot = pending_slot and decrements attempts.
  // - If already on pending_slot and not marked_good:
  //     - if boot_attempts_left > 0: decrements attempts.
  //     - if boot_attempts_left == 0: rolls back to the other slot, clears
  //       pending_slot, marks good.
  Status on_boot();

  // Confirms the currently running slot is healthy. Clears pending state and
  // sets marked_good = true.
  Status mark_good();

  std::string active_slot() const { return state_.active_slot; }
  std::string pending_slot() const { return state_.pending_slot; }
  uint32_t boot_attempts_left() const noexcept {
    return state_.boot_attempts_left;
  }
  bool is_marked_good() const noexcept { return state_.marked_good; }
  const hal::BootSlotState& state() const noexcept { return state_; }

  static std::string other_slot(std::string_view slot);

 private:
  hal::IBootControlHal& boot_control_;
  hal::IStorageHal& storage_;
  hal::BootSlotState state_;
};

}  // namespace aegis::ota

#endif  // AEGIS_OTA_AB_SLOT_MANAGER_H_
