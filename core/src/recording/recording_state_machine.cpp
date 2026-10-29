#include "aegis/recording/recording_state_machine.h"

namespace aegis::recording {

const char* state_to_string(State state) noexcept {
  switch (state) {
    case State::kIdle:
      return "Idle";
    case State::kBuffering:
      return "Buffering";
    case State::kRecording:
      return "Recording";
    case State::kPostRecord:
      return "PostRecord";
    case State::kFault:
      return "Fault";
  }
  return "Unknown";
}

RecordingStateMachine::RecordingStateMachine(uint64_t segment_duration_ms,
                                             uint64_t post_record_ms) noexcept
    : segment_duration_ms_(segment_duration_ms > 0 ? segment_duration_ms : 1),
      post_record_ms_(post_record_ms) {}

TransitionResult RecordingStateMachine::dispatch(const Event& event) {
  TransitionResult out;
  out.prev_state = state_;
  out.next_state = state_;

  // Global fault transition from any state on StorageFull
  if (event.type == EventType::kStorageFull) {
    if (state_ == State::kRecording || state_ == State::kPostRecord) {
      if (segment_elapsed_ms_ > 0) {
        out.actions.push_back(
            Action{ActionType::kCutSegment, "storage_full_flush"});
      }
      out.actions.push_back(
          Action{ActionType::kSealIncident, "storage_full_seal"});
    }
    segment_elapsed_ms_ = 0;
    post_record_elapsed_ms_ = 0;
    active_source_.clear();
    state_ = State::kFault;
    out.next_state = state_;
    out.actions.push_back(Action{ActionType::kRaiseFault, "storage_full"});
    return out;
  }

  switch (state_) {
    case State::kIdle:
      if (event.type == EventType::kPowerOn) {
        state_ = State::kBuffering;
        out.actions.push_back(
            Action{ActionType::kStartPreEventBuffer, "power_on"});
      }
      break;

    case State::kBuffering:
      if (event.type == EventType::kTrigger) {
        state_ = State::kRecording;
        segment_elapsed_ms_ = 0;
        post_record_elapsed_ms_ = 0;
        active_source_ = event.source;
        out.actions.push_back(Action{ActionType::kBeginIncident, event.source});
        out.actions.push_back(
            Action{ActionType::kFlushPreEventBuffer, event.source});
      }
      break;

    case State::kRecording:
      if (event.type == EventType::kTick) {
        segment_elapsed_ms_ += event.delta_ms;
        while (segment_elapsed_ms_ >= segment_duration_ms_) {
          segment_elapsed_ms_ -= segment_duration_ms_;
          out.actions.push_back(Action{ActionType::kCutSegment, "periodic"});
        }
      } else if (event.type == EventType::kStop) {
        if (post_record_ms_ > 0) {
          state_ = State::kPostRecord;
          post_record_elapsed_ms_ = 0;
          out.actions.push_back(
              Action{ActionType::kEnterPostRecord, event.source});
        } else {
          state_ = State::kBuffering;
          segment_elapsed_ms_ = 0;
          active_source_.clear();
          out.actions.push_back(Action{ActionType::kCutSegment, "stop_flush"});
          out.actions.push_back(Action{ActionType::kSealIncident, event.source});
        }
      }
      break;

    case State::kPostRecord:
      if (event.type == EventType::kTrigger) {
        state_ = State::kRecording;
        post_record_elapsed_ms_ = 0;
        active_source_ = event.source;
        out.actions.push_back(
            Action{ActionType::kResumeRecording, event.source});
      } else if (event.type == EventType::kTick) {
        uint64_t remaining = event.delta_ms;
        if (post_record_elapsed_ms_ + remaining >= post_record_ms_) {
          const uint64_t until_end = post_record_ms_ - post_record_elapsed_ms_;
          segment_elapsed_ms_ += until_end;
          while (segment_elapsed_ms_ >= segment_duration_ms_) {
            segment_elapsed_ms_ -= segment_duration_ms_;
            out.actions.push_back(Action{ActionType::kCutSegment, "periodic"});
          }
          if (segment_elapsed_ms_ > 0) {
            out.actions.push_back(
                Action{ActionType::kCutSegment, "post_record_final"});
          }
          segment_elapsed_ms_ = 0;
          post_record_elapsed_ms_ = 0;
          active_source_.clear();
          state_ = State::kBuffering;
          out.actions.push_back(
              Action{ActionType::kSealIncident, "post_record_complete"});
        } else {
          post_record_elapsed_ms_ += remaining;
          segment_elapsed_ms_ += remaining;
          while (segment_elapsed_ms_ >= segment_duration_ms_) {
            segment_elapsed_ms_ -= segment_duration_ms_;
            out.actions.push_back(Action{ActionType::kCutSegment, "periodic"});
          }
        }
      }
      break;

    case State::kFault:
      if (event.type == EventType::kFaultCleared) {
        state_ = State::kBuffering;
        out.actions.push_back(Action{ActionType::kClearFault, "fault_cleared"});
        out.actions.push_back(
            Action{ActionType::kStartPreEventBuffer, "fault_recovered"});
      }
      break;
  }

  out.next_state = state_;
  return out;
}

}  // namespace aegis::recording
