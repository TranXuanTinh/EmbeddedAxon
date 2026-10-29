#ifndef AEGIS_HAL_SIM_SIM_CAMERA_HAL_H_
#define AEGIS_HAL_SIM_SIM_CAMERA_HAL_H_

#include <cstdint>

#include "aegis/common/clock.h"
#include "aegis/hal/i_camera_hal.h"

namespace aegis::hal::sim {

class SimCameraHal final : public ICameraHal {
 public:
  SimCameraHal(const common::IClock& clock, uint32_t fps = 30,
               uint32_t keyframe_interval = 30) noexcept;

  Result<evidence::Frame> read_frame() override;

  uint64_t next_seq() const noexcept { return next_seq_; }

 private:
  const common::IClock& clock_;
  uint32_t fps_;
  uint32_t keyframe_interval_;
  uint64_t start_ts_ms_;
  uint64_t next_seq_{0};
};

}  // namespace aegis::hal::sim

#endif  // AEGIS_HAL_SIM_SIM_CAMERA_HAL_H_
