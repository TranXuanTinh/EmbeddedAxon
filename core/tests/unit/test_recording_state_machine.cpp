#include <gtest/gtest.h>

#include "aegis/recording/recording_state_machine.h"

namespace aegis::recording {
namespace {

TEST(RecordingStateMachineTest, FullLifecyclePowerOnTriggerSegmentPostRecord) {
  RecordingStateMachine sm(10000, 5000);
  EXPECT_EQ(sm.state(), State::kIdle);

  auto r1 = sm.dispatch(Event::PowerOn());
  EXPECT_EQ(r1.next_state, State::kBuffering);
  ASSERT_EQ(r1.actions.size(), 1U);
  EXPECT_EQ(r1.actions[0].type, ActionType::kStartPreEventBuffer);

  auto r2 = sm.dispatch(Event::Trigger("holster"));
  EXPECT_EQ(r2.next_state, State::kRecording);
  ASSERT_EQ(r2.actions.size(), 2U);
  EXPECT_EQ(r2.actions[0].type, ActionType::kBeginIncident);
  EXPECT_EQ(r2.actions[1].type, ActionType::kFlushPreEventBuffer);

  auto r3 = sm.dispatch(Event::Tick(10000));
  EXPECT_EQ(r3.next_state, State::kRecording);
  ASSERT_EQ(r3.actions.size(), 1U);
  EXPECT_EQ(r3.actions[0].type, ActionType::kCutSegment);

  auto r4 = sm.dispatch(Event::Stop("holster"));
  EXPECT_EQ(r4.next_state, State::kPostRecord);
  ASSERT_EQ(r4.actions.size(), 1U);
  EXPECT_EQ(r4.actions[0].type, ActionType::kEnterPostRecord);

  // Trigger during PostRecord resumes Recording
  auto r5 = sm.dispatch(Event::Trigger("button"));
  EXPECT_EQ(r5.next_state, State::kRecording);
  ASSERT_EQ(r5.actions.size(), 1U);
  EXPECT_EQ(r5.actions[0].type, ActionType::kResumeRecording);

  // Stop again and let PostRecord expire (5000 ms)
  (void)sm.dispatch(Event::Stop("button"));
  auto r6 = sm.dispatch(Event::Tick(5000));
  EXPECT_EQ(r6.next_state, State::kBuffering);
  ASSERT_GE(r6.actions.size(), 1U);
  EXPECT_EQ(r6.actions.back().type, ActionType::kSealIncident);
}

TEST(RecordingStateMachineTest, StorageFullTransitionsToFaultAndRecovers) {
  RecordingStateMachine sm(10000, 5000);
  (void)sm.dispatch(Event::PowerOn());
  (void)sm.dispatch(Event::Trigger("button"));
  (void)sm.dispatch(Event::Tick(3000));

  auto rf = sm.dispatch(Event::StorageFull());
  EXPECT_EQ(rf.next_state, State::kFault);
  ASSERT_EQ(rf.actions.size(), 3U);
  EXPECT_EQ(rf.actions[0].type, ActionType::kCutSegment);
  EXPECT_EQ(rf.actions[1].type, ActionType::kSealIncident);
  EXPECT_EQ(rf.actions[2].type, ActionType::kRaiseFault);

  auto rc = sm.dispatch(Event::FaultCleared());
  EXPECT_EQ(rc.next_state, State::kBuffering);
}

}  // namespace
}  // namespace aegis::recording
