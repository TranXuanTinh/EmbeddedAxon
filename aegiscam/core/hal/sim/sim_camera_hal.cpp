#include "hal/sim/sim_camera_hal.h"

#include <cstdio>
#include <string>

namespace aegis::hal::sim {

SimCameraHal::SimCameraHal(const common::IClock& clock, uint32_t fps,
                           uint32_t keyframe_interval) noexcept
    : clock_(clock),
      fps_(fps > 0 ? fps : 30),
      keyframe_interval_(keyframe_interval > 0 ? keyframe_interval : 30),
      start_ts_ms_(clock.now_ms()) {}

Result<evidence::Frame> SimCameraHal::read_frame() {
  const uint64_t now = clock_.now_ms();
  const uint64_t frame_ts =
      start_ts_ms_ + (next_seq_ * 1000ULL) / static_cast<uint64_t>(fps_);
  if (frame_ts > now) {
    return Result<evidence::Frame>::Error(ErrorCode::kEmpty,
                                          "No camera frame ready yet");
  }

  evidence::Frame frame;
  frame.seq = next_seq_;
  frame.ts_ms = frame_ts;
  frame.keyframe = (next_seq_ % keyframe_interval_) == 0;

  char buf[64];
  const int n = std::snprintf(
      buf, sizeof(buf), "AEGIS_SIM_FRAME:seq=%llu:ts=%llu:kf=%d",
      static_cast<unsigned long long>(frame.seq),
      static_cast<unsigned long long>(frame.ts_ms), frame.keyframe ? 1 : 0);
  frame.payload.assign(reinterpret_cast<const uint8_t*>(buf),
                       reinterpret_cast<const uint8_t*>(buf) + (n > 0 ? n : 0));

  ++next_seq_;
  return Result<evidence::Frame>::Ok(std::move(frame));
}

}  // namespace aegis::hal::sim
