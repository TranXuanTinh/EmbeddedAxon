#include "hal/sim/sim_key_store_hal.h"

namespace aegis::hal::sim {

SimKeyStoreHal::SimKeyStoreHal(std::string_view key)
    : key_(reinterpret_cast<const uint8_t*>(key.data()),
           reinterpret_cast<const uint8_t*>(key.data()) + key.size()) {}

Result<std::vector<uint8_t>> SimKeyStoreHal::get_device_hmac_key() const {
  return Result<std::vector<uint8_t>>::Ok(key_);
}

}  // namespace aegis::hal::sim
