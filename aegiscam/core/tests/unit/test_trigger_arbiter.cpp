#include <gtest/gtest.h>

#include "aegis/trigger/trigger_arbiter.h"

namespace aegis::trigger {
namespace {

TEST(TriggerArbiterTest, DebouncesRapidDuplicateTriggersFromSameSource) {
  TriggerArbiter arbiter(50);

  EXPECT_EQ(arbiter.evaluate_trigger(TriggerSource::kButton, 1000),
            ArbiterDecision::kAccepted);
  EXPECT_EQ(arbiter.evaluate_trigger(TriggerSource::kButton, 1020),
            ArbiterDecision::kDebounced);
  EXPECT_EQ(arbiter.evaluate_trigger(TriggerSource::kButton, 1055),
            ArbiterDecision::kAccepted);
}

TEST(TriggerArbiterTest, LowerPriorityCannotStopHigherPriorityRecording) {
  TriggerArbiter arbiter(50);

  EXPECT_EQ(arbiter.evaluate_trigger(TriggerSource::kHolster, 1000),
            ArbiterDecision::kAccepted);

  // Remote and button have lower priority than holster -> rejected
  EXPECT_EQ(arbiter.evaluate_stop(TriggerSource::kRemote, 2000),
            ArbiterDecision::kRejectedLowerPriority);
  EXPECT_EQ(arbiter.evaluate_stop(TriggerSource::kSignal, 2000),
            ArbiterDecision::kRejectedLowerPriority);
  EXPECT_EQ(arbiter.evaluate_stop(TriggerSource::kButton, 2000),
            ArbiterDecision::kRejectedLowerPriority);

  // Holster can stop holster
  EXPECT_EQ(arbiter.evaluate_stop(TriggerSource::kHolster, 2000),
            ArbiterDecision::kAccepted);
}

TEST(TriggerArbiterTest, HigherPriorityTriggerUpgradesActiveSource) {
  TriggerArbiter arbiter(50);

  EXPECT_EQ(arbiter.evaluate_trigger(TriggerSource::kRemote, 1000),
            ArbiterDecision::kAccepted);
  EXPECT_EQ(arbiter.evaluate_trigger(TriggerSource::kHolster, 1100),
            ArbiterDecision::kAccepted);

  // Now remote can no longer stop the recording
  EXPECT_EQ(arbiter.evaluate_stop(TriggerSource::kRemote, 1200),
            ArbiterDecision::kRejectedLowerPriority);
}

}  // namespace
}  // namespace aegis::trigger
