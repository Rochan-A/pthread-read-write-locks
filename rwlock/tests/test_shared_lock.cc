#include <atomic>
#include <cassert>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>

#include "rwlock/rw_lock.h"
#include "rwlock/shared_lock.h"
#include "rwlock/unique_lock.h"

// Test multiple threads can hold the shared lock.
TEST(SharedLockTest, TestMultipleReaders) {
  rwlock::RWLock rwlock;
  std::atomic<bool> keep_reading{true};

  const int shared_data = 42;
  const int num_threads = 5;

  std::atomic<int> active_readers{0};

  auto reader_func = [&](int idx) {
    rwlock::SharedLock shared_lock(rwlock);

    if (shared_data == 42) {
      active_readers.fetch_add(1, std::memory_order_relaxed);
      while (keep_reading.load(std::memory_order_relaxed)) {
        std::this_thread::yield();
      }
    }
    active_readers.fetch_sub(1, std::memory_order_relaxed);
  };

  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(reader_func, i);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  ASSERT_EQ(active_readers.load(), num_threads);
  keep_reading.store(false);

  for (auto &t : threads) {
    t.join();
  }

  ASSERT_EQ(active_readers.load(), 0);
}

// Exclusive lock blocks readers.
TEST(SharedLockTest, TestExclusiveLockBlocksReaders) {
  rwlock::RWLock lock;

  int shared_data = 42;
  std::atomic<int> active_readers{0};
  std::atomic<bool> keep_reading{true};

  const int first_readers = 3;
  const int second_readers = 2;
  bool writer_blocked = false;

  auto reader_func = [&](bool first_round) {
    rwlock::SharedLock shared_lock(lock);
    ASSERT_TRUE(shared_lock.owns_lock());

    active_readers.fetch_add(1, std::memory_order_relaxed);
    if (first_round) {
      EXPECT_EQ(shared_data, 42);
    } else {
      EXPECT_EQ(shared_data, 43);
    }
    while (keep_reading.load(std::memory_order_relaxed)) {
      std::this_thread::yield();
    }
    active_readers.fetch_sub(1, std::memory_order_relaxed);
  };

  auto writer_func = [&]() {
    int start = active_readers.load(std::memory_order_relaxed);
    if (start > 0) {
      writer_blocked = true;
    }

    rwlock::UniqueLock unique_lock(lock);
    ASSERT_TRUE(unique_lock.owns_lock());
    ASSERT_EQ(active_readers.load(), 0);
    ++shared_data;
  };

  std::vector<std::thread> threads;
  threads.reserve(first_readers + 1 + second_readers);

  for (int i = 0; i < first_readers; ++i) {
    threads.emplace_back(reader_func, true);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  threads.emplace_back(writer_func);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  keep_reading.store(false);
  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
  threads.clear();

  // shared_data is 43. Spawn second readers verifying they see updated data.
  keep_reading.store(true);
  threads.reserve(second_readers);
  for (int i = 0; i < second_readers; ++i) {
    threads.emplace_back(reader_func, false);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  keep_reading.store(false);

  for (auto &t : threads) {
    t.join();
  }
  EXPECT_TRUE(writer_blocked);
}
