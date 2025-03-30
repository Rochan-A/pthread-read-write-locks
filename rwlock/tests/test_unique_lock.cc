#include <atomic>
#include <cassert>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>

#include "rwlock/rw_lock.h"
#include "rwlock/unique_lock.h"

TEST(UniqueLockTest, TestMultipleWriters) {
  rwlock::RWLock lock;
  std::atomic<int> sim_writers{0}, max_writers{0};
  const int n = 5;

  auto writer = [&]() {
    rwlock::UniqueLock unique_lock(lock);
    ASSERT_TRUE(unique_lock.owns_lock());
    int c = sim_writers.fetch_add(1, std::memory_order_acq_rel) + 1;
    int m = max_writers.load(std::memory_order_relaxed);
    while (c > m) {
      max_writers.compare_exchange_weak(m, c);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sim_writers.fetch_sub(1, std::memory_order_acq_rel);
  };

  std::vector<std::thread> threads(n);
  for (auto &t : threads) {
    t = std::thread(writer);
  }
  for (auto &t : threads) {
    t.join();
  }
  EXPECT_EQ(max_writers.load(), 1);
}
