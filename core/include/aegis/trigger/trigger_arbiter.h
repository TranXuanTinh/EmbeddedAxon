#ifndef AEGIS_TRIGGER_TRIGGER_ARBITER_H_
#define AEGIS_TRIGGER_TRIGGER_ARBITER_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "aegis/common/result.h"

namespace aegis::trigger {

// Trigger sources ordered by priority (highest to lowest):
// holster (40) > button (30) > signal (20) > remote (10)
enum class TriggerSource {
  kRemote = 10,
  kSignal = 20,
  kButton = 30,
  kHolster = 40,
};

Result<TriggerSource> parse_trigger_source(std::string_view src) noexcept;
const char* trigger_source_to_string(TriggerSource src) noexcept;
int trigger_priority(TriggerSource src) noexcept;

enum class ArbiterDecision {
  kAccepted = 0,
  kDebounced,
  kRejectedLowerPriority,
};

class TriggerArbiter {
 public:
  explicit TriggerArbiter(uint64_t debounce_ms = 50) noexcept;

  // Evaluates a TRIGGER event from `src` at `now_ms`.
  // Upgrades active_source if `src` has higher priority than current active.
  ArbiterDecision evaluate_trigger(TriggerSource src, uint64_t now_ms);

  // Evaluates a STOP event from `src` at `now_ms`.
  // Rejects STOP if `src` has lower priority than the active trigger source.
  ArbiterDecision evaluate_stop(TriggerSource src, uint64_t now_ms);

  // Resets active recording source (e.g. when incident seals).
  void clear_active() noexcept;

  std::optional<TriggerSource> active_source() const noexcept {
    return active_source_;
  }

 private:
  uint64_t debounce_ms_;
  std::unordered_map<TriggerSource, uint64_t> last_trigger_ts_;
  std::optional<TriggerSource> active_source_;
};

}  // namespace aegis::trigger

#endif  // AEGIS_TRIGGER_TRIGGER_ARBITER_H_
