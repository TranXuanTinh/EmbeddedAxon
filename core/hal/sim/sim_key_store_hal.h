#ifndef AEGIS_HAL_SIM_SIM_KEY_STORE_HAL_H_
#define AEGIS_HAL_SIM_SIM_KEY_STORE_HAL_H_

#include <string>
#include <string_view>
#include <vector>

#include "aegis/hal/i_key_store_hal.h"

namespace aegis::hal::sim {

class SimKeyStoreHal final : public IKeyStoreHal {
 public:
  static constexpr std::string_view kDefaultSimKey =
      "aegis-sim-device-key-DO-NOT-SHIP";

  explicit SimKeyStoreHal(
      std::string_view key = kDefaultSimKey);

  Result<std::vector<uint8_t>> get_device_hmac_key() const override;

 private:
  std::vector<uint8_t> key_;
};

}  // namespace aegis::hal::sim

#endif  // AEGIS_HAL_SIM_SIM_KEY_STORE_HAL_H_
