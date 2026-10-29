#include <gtest/gtest.h>

#include <cstdint>
#include <thread>

#include "aegis/util/spsc_ring_buffer.h"

namespace aegis::util {
namespace {

TEST(SpscRingBufferTest, PushPopOrderAndCapacityBounds) {
  SpscRingBuffer<int, 4> ring;
  EXPECT_TRUE(ring.empty());
  EXPECT_EQ(ring.capacity(), 4U);

  EXPECT_TRUE(ring.try_push(10));
  EXPECT_TRUE(ring.try_push(20));
  EXPECT_TRUE(ring.try_push(30));
  EXPECT_TRUE(ring.try_push(40));
  EXPECT_TRUE(ring.full());
  EXPECT_FALSE(ring.try_push(50));

  EXPECT_EQ(ring.try_pop(), 10);
  EXPECT_EQ(ring.try_pop(), 20);
  EXPECT_TRUE(ring.try_push(50));
  EXPECT_EQ(ring.try_pop(), 30);
  EXPECT_EQ(ring.try_pop(), 40);
  EXPECT_EQ(ring.try_pop(), 50);
  EXPECT_FALSE(ring.try_pop().has_value());
}

TEST(SpscRingBufferTest, ConcurrentProducerConsumerMaintainsStrictOrder) {
  constexpr uint64_t kTotalItems = 50000;
  SpscRingBuffer<uint64_t, 256> ring;

  std::thread producer([&]() {
    for (uint64_t i = 1; i <= kTotalItems; ++i) {
      while (!ring.try_push(i)) {
        std::this_thread::yield();
      }
    }
  });

  uint64_t sum = 0;
  uint64_t expected = 1;
  std::thread consumer([&]() {
    while (expected <= kTotalItems) {
      uint64_t val = 0;
      if (ring.try_pop(val)) {
        ASSERT_EQ(val, expected);
        sum += val;
        ++expected;
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();
  EXPECT_EQ(expected, kTotalItems + 1);
  EXPECT_EQ(sum, (kTotalItems * (kTotalItems + 1)) / 2);
}

}  // namespace
}  // namespace aegis::util
