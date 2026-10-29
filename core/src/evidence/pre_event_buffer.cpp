#include "aegis/evidence/pre_event_buffer.h"

#include <utility>

namespace aegis::evidence {

PreEventBuffer::PreEventBuffer(uint64_t window_ms, std::size_t max_frames)
    : window_ms_(window_ms), max_frames_(max_frames > 0 ? max_frames : 1) {}

void PreEventBuffer::evict_expired_locked(uint64_t newest_ts_ms) {
  while (frames_.size() > max_frames_) {
    frames_.pop_front();
  }
  if (window_ms_ == 0) {
    while (frames_.size() > 1) {
      frames_.pop_front();
    }
    return;
  }
  while (!frames_.empty() && newest_ts_ms >= frames_.front().ts_ms &&
         (newest_ts_ms - frames_.front().ts_ms) > window_ms_) {
    frames_.pop_front();
  }
}

void PreEventBuffer::push(Frame frame) {
  std::lock_guard<std::mutex> lock(mutex_);
  const uint64_t ts = frame.ts_ms;
  frames_.push_back(std::move(frame));
  evict_expired_locked(ts);
}

std::vector<Frame> PreEventBuffer::drain() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (frames_.empty()) {
    return {};
  }
  evict_expired_locked(frames_.back().ts_ms);

  // Drop leading non-keyframes so playback always starts at a keyframe.
  while (!frames_.empty() && !frames_.front().keyframe) {
    frames_.pop_front();
  }

  std::vector<Frame> out;
  out.reserve(frames_.size());
  while (!frames_.empty()) {
    out.push_back(std::move(frames_.front()));
    frames_.pop_front();
  }
  return out;
}

void PreEventBuffer::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  frames_.clear();
}

std::size_t PreEventBuffer::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return frames_.size();
}

bool PreEventBuffer::empty() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return frames_.empty();
}

}  // namespace aegis::evidence
