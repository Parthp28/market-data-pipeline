#include "mdp/spsc_queue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

TEST(Spsc, PushPopSingleThread) {
  mdp::SpscQueue<int, 8> q;
  EXPECT_TRUE(q.try_push(1));
  EXPECT_TRUE(q.try_push(2));
  int v = 0;
  EXPECT_TRUE(q.try_pop(v));
  EXPECT_EQ(v, 1);
  EXPECT_TRUE(q.try_pop(v));
  EXPECT_EQ(v, 2);
  EXPECT_FALSE(q.try_pop(v));
}

TEST(Spsc, FillsAndRejects) {
  mdp::SpscQueue<int, 4> q;
  EXPECT_TRUE(q.try_push(1));
  EXPECT_TRUE(q.try_push(2));
  EXPECT_TRUE(q.try_push(3));
  EXPECT_TRUE(q.try_push(4));
  EXPECT_FALSE(q.try_push(5));
}

TEST(Spsc, ConcurrentRoundTrip) {
  mdp::SpscQueue<int, 1024> q;
  constexpr int N = 50000;
  std::atomic<long long> sum{0};
  std::thread prod([&] {
    for (int i = 1; i <= N; ++i) {
      while (!q.try_push(i)) {
      }
    }
  });
  std::thread cons([&] {
    int got = 0;
    int v = 0;
    while (got < N) {
      if (q.try_pop(v)) {
        sum.fetch_add(v, std::memory_order_relaxed);
        ++got;
      }
    }
  });
  prod.join();
  cons.join();
  EXPECT_EQ(sum.load(), static_cast<long long>(N) * (N + 1) / 2);
}
