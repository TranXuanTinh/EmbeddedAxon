#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "aegis/util/bounded_queue.h"

namespace aegis::util {
namespace {

TEST(BoundedQueueTest, RejectPolicyRefusesWhenFull) {
  BoundedQueue<int> q(2, OverflowPolicy::kReject);
  EXPECT_TRUE(q.try_push(1));
  EXPECT_TRUE(q.try_push(2));
  EXPECT_FALSE(q.try_push(3));
  EXPECT_EQ(q.rejected_count(), 1U);

  EXPECT_EQ(q.try_pop(), 1);
  EXPECT_EQ(q.try_pop(), 2);
  EXPECT_FALSE(q.try_pop().has_value());
}

TEST(BoundedQueueTest, DropOldestPolicyEvictsOldestElement) {
  BoundedQueue<int> q(2, OverflowPolicy::kDropOldest);
  EXPECT_TRUE(q.try_push(1));
  EXPECT_TRUE(q.try_push(2));
  EXPECT_TRUE(q.try_push(3));
  EXPECT_EQ(q.dropped_count(), 1U);

  EXPECT_EQ(q.try_pop(), 2);
  EXPECT_EQ(q.try_pop(), 3);
}

TEST(BoundedQueueTest, CloseUnblocksWaitingConsumers) {
  BoundedQueue<int> q(4, OverflowPolicy::kReject);
  bool consumer_exited = false;

  std::thread consumer([&]() {
    const auto item = q.pop();
    EXPECT_FALSE(item.has_value());
    consumer_exited = true;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  q.close();
  consumer.join();
  EXPECT_TRUE(consumer_exited);
  EXPECT_FALSE(q.try_push(42));
}

}  // namespace
}  // namespace aegis::util
