#ifndef AEGIS_EVIDENCE_PRE_EVENT_BUFFER_H_
#define AEGIS_EVIDENCE_PRE_EVENT_BUFFER_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

#include "aegis/evidence/frame.h"

namespace aegis::evidence {

// Bounded circular pre-event frame buffer.
// Retains frames within [latest_ts - window_ms, latest_ts] up to max_frames.
// On drain(), returns buffered frames starting at the first keyframe within
// the retention window so video decoders always begin on an IDR/keyframe.
class PreEventBuffer {
 public:
  explicit PreEventBuffer(uint64_t window_ms, std::size_t max_frames = 4096);

  void push(Frame frame);

  // Drains and clears the buffer, returning frames starting from the earliest
  // keyframe inside the window. If no keyframe exists, returns an empty vector.
  std::vector<Frame> drain();

  void clear();

  std::size_t size() const;
  bool empty() const;
  uint64_t window_ms() const noexcept { return window_ms_; }

 private:
  void evict_expired_locked(uint64_t newest_ts_ms);

  uint64_t window_ms_;
  std::size_t max_frames_;
  mutable std::mutex mutex_;
  std::deque<Frame> frames_;
};

}  // namespace aegis::evidence

#endif  // AEGIS_EVIDENCE_PRE_EVENT_BUFFER_H_
