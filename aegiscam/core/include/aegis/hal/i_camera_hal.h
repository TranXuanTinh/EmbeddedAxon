#ifndef AEGIS_HAL_I_CAMERA_HAL_H_
#define AEGIS_HAL_I_CAMERA_HAL_H_

#include "aegis/common/result.h"
#include "aegis/evidence/frame.h"

namespace aegis::hal {

class ICameraHal {
 public:
  virtual ~ICameraHal() = default;

  // Reads the next available frame. Returns kEmpty when no new frame is ready.
  virtual Result<evidence::Frame> read_frame() = 0;
};

}  // namespace aegis::hal

#endif  // AEGIS_HAL_I_CAMERA_HAL_H_
