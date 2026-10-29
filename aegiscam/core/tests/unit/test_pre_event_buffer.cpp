#include <gtest/gtest.h>

#include "aegis/evidence/pre_event_buffer.h"

namespace aegis::evidence {
namespace {

Frame make_frame(uint64_t seq, uint64_t ts_ms, bool keyframe) {
  Frame f;
  f.seq = seq;
  f.ts_ms = ts_ms;
  f.keyframe = keyframe;
  f.payload = {static_cast<uint8_t>(seq & 0xFFU)};
  return f;
}

TEST(PreEventBufferTest, DrainStartsAtEarliestKeyframeWithinWindow) {
  PreEventBuffer buf(3000, 100);  // 3-second retention window

  // Push frames from t=0 to t=5000 ms (window at t=5000 is [2000, 5000])
  buf.push(make_frame(0, 0, true));       // evicted (> 3000 ms old)
  buf.push(make_frame(1, 1000, false));   // evicted
  buf.push(make_frame(2, 2000, false));   // in window, but NOT a keyframe
  buf.push(make_frame(3, 2500, false));   // in window, NOT a keyframe
  buf.push(make_frame(4, 3000, true));    // first keyframe in window!
  buf.push(make_frame(5, 4000, false));
  buf.push(make_frame(6, 5000, false));

  const auto drained = buf.drain();
  ASSERT_EQ(drained.size(), 3U);
  EXPECT_TRUE(drained.front().keyframe);
  EXPECT_EQ(drained[0].seq, 4U);
  EXPECT_EQ(drained[1].seq, 5U);
  EXPECT_EQ(drained[2].seq, 6U);
  EXPECT_TRUE(buf.empty());
}

TEST(PreEventBufferTest, DrainReturnsEmptyWhenNoKeyframePresent) {
  PreEventBuffer buf(5000, 100);
  buf.push(make_frame(1, 100, false));
  buf.push(make_frame(2, 200, false));

  const auto drained = buf.drain();
  EXPECT_TRUE(drained.empty());
}

}  // namespace
}  // namespace aegis::evidence
