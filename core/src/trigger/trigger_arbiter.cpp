#include "aegis/trigger/trigger_arbiter.h"

namespace aegis::trigger {

Result<TriggerSource> parse_trigger_source(std::string_view src) noexcept {
  if (src == "holster") {
    return Result<TriggerSource>::Ok(TriggerSource::kHolster);
  }
  if (src == "button") {
    return Result<TriggerSource>::Ok(TriggerSource::kButton);
  }
  if (src == "signal") {
    return Result<TriggerSource>::Ok(TriggerSource::kSignal);
  }
  if (src == "remote") {
    return Result<TriggerSource>::Ok(TriggerSource::kRemote);
  }
  return Result<TriggerSource>::Error(
      ErrorCode::kInvalidArgument,
      "Unknown trigger source (expected button|holster|signal|remote)");
}

const char* trigger_source_to_string(TriggerSource src) noexcept {
  switch (src) {
    case TriggerSource::kHolster:
      return "holster";
    case TriggerSource::kButton:
      return "button";
    case TriggerSource::kSignal:
      return "signal";
    case TriggerSource::kRemote:
      return "remote";
  }
  return "unknown";
}

int trigger_priority(TriggerSource src) noexcept {
  return static_cast<int>(src);
}

TriggerArbiter::TriggerArbiter(uint64_t debounce_ms) noexcept
    : debounce_ms_(debounce_ms) {}

ArbiterDecision TriggerArbiter::evaluate_trigger(TriggerSource src,
                                                 uint64_t now_ms) {
  auto it = last_trigger_ts_.find(src);
  if (it != last_trigger_ts_.end() && now_ms >= it->second &&
      (now_ms - it->second) < debounce_ms_) {
    return ArbiterDecision::kDebounced;
  }
  last_trigger_ts_[src] = now_ms;

  if (!active_source_.has_value() ||
      trigger_priority(src) > trigger_priority(*active_source_)) {
    active_source_ = src;
  }
  return ArbiterDecision::kAccepted;
}

ArbiterDecision TriggerArbiter::evaluate_stop(TriggerSource src,
                                              uint64_t /*now_ms*/) {
  if (!active_source_.has_value()) {
    return ArbiterDecision::kAccepted;
  }
  if (trigger_priority(src) < trigger_priority(*active_source_)) {
    return ArbiterDecision::kRejectedLowerPriority;
  }
  return ArbiterDecision::kAccepted;
}

void TriggerArbiter::clear_active() noexcept { active_source_.reset(); }

}  // namespace aegis::trigger
