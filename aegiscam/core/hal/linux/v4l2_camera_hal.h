#ifndef AEGIS_HAL_LINUX_V4L2_CAMERA_HAL_H_
#define AEGIS_HAL_LINUX_V4L2_CAMERA_HAL_H_

#include <cstdint>
#include <string>

#include "aegis/common/clock.h"
#include "aegis/hal/i_camera_hal.h"

namespace aegis::hal::linux_hal {

// Video4Linux2 (V4L2) camera HAL skeleton for embedded Linux CSI/USB sensors.
class V4l2CameraHal final : public ICameraHal {
 public:
  V4l2CameraHal(std::string device_node, const common::IClock& clock);
  ~V4l2CameraHal() override;

  V4l2CameraHal(const V4l2CameraHal&) = delete;
  V4l2CameraHal& operator=(const V4l2CameraHal&) = delete;

  Status open_device();
  void close_device() noexcept;

  Result<evidence::Frame> read_frame() override;

 private:
  std::string device_node_;
  const common::IClock& clock_;
  int fd_{-1};
  uint64_t seq_{0};
};

}  // namespace aegis::hal::linux_hal

#endif  // AEGIS_HAL_LINUX_V4L2_CAMERA_HAL_H_
