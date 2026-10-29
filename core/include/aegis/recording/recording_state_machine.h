#ifndef AEGIS_RECORDING_RECORDING_STATE_MACHINE_H_
#define AEGIS_RECORDING_RECORDING_STATE_MACHINE_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aegis::recording {

enum class State {
  kIdle = 0,
  kBuffering,
  kRecording,
  kPostRecord,
  kFault,
};

const char* state_to_string(State state) noexcept;

enum class EventType {
  kPowerOn = 0,
  kTrigger,
  kStop,
  kTick,
  kStorageFull,
  kFaultCleared,
};

struct Event {
  EventType type{EventType::kTick};
  std::string source;
  uint64_t delta_ms{0};

  static Event PowerOn() { return Event{EventType::kPowerOn, {}, 0}; }
  static Event Trigger(std::string_view src) {
    return Event{EventType::kTrigger, std::string(src), 0};
  }
  static Event Stop(std::string_view src) {
    return Event{EventType::kStop, std::string(src), 0};
  }
  static Event Tick(uint64_t ms) { return Event{EventType::kTick, {}, ms}; }
  static Event StorageFull() { return Event{EventType::kStorageFull, {}, 0}; }
  static Event FaultCleared() { return Event{EventType::kFaultCleared, {}, 0}; }
};

enum class ActionType {
  kStartPreEventBuffer = 0,
  kBeginIncident,
  kFlushPreEventBuffer,
  kCutSegment,
  kEnterPostRecord,
  kResumeRecording,
  kSealIncident,
  kRaiseFault,
  kClearFault,
};

struct Action {
  ActionType type;
  std::string detail;
};

struct TransitionResult {
  State prev_state{State::kIdle};
  State next_state{State::kIdle};
  std::vector<Action> actions;
};

class RecordingStateMachine {
 public:
  RecordingStateMachine(uint64_t segment_duration_ms = 10000,
                        uint64_t post_record_ms = 5000) noexcept;

  TransitionResult dispatch(const Event& event);

  State state() const noexcept { return state_; }
  uint64_t segment_elapsed_ms() const noexcept { return segment_elapsed_ms_; }
  uint64_t post_record_elapsed_ms() const noexcept {
    return post_record_elapsed_ms_;
  }
  const std::string& active_trigger_source() const noexcept {
    return active_source_;
  }

 private:
  State state_{State::kIdle};
  uint64_t segment_duration_ms_;
  uint64_t post_record_ms_;
  uint64_t segment_elapsed_ms_{0};
  uint64_t post_record_elapsed_ms_{0};
  std::string active_source_;
};

}  // namespace aegis::recording

#endif  // AEGIS_RECORDING_RECORDING_STATE_MACHINE_H_
