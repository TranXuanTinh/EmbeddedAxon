#ifndef AEGIS_HAL_I_KEY_STORE_HAL_H_
#define AEGIS_HAL_I_KEY_STORE_HAL_H_

#include <cstdint>
#include <vector>

#include "aegis/common/result.h"

namespace aegis::hal {

class IKeyStoreHal {
 public:
  virtual ~IKeyStoreHal() = default;

  // Returns the device-specific secret key used for HMAC signing of manifests.
  // In production, backed by TPM 2.0 / NXP EdgeLock secure element.
  virtual Result<std::vector<uint8_t>> get_device_hmac_key() const = 0;
};

}  // namespace aegis::hal

#endif  // AEGIS_HAL_I_KEY_STORE_HAL_H_
