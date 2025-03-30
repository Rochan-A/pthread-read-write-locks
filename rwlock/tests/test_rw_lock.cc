#include <atomic>
#include <cassert>
#include <cstdlib>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>

#include "rwlock/rw_lock.h"

// Test multiple threads can hold the shared lock.
TEST(RwLockTest, TestMultipleReaders) {
  rwlock::RWLock rwlock;
  std::atomic<bool> keep_reading{true};

  const int shared_data = 42;
  const int num_threads = 5;

  std::atomic<int> active_readers{0};

  auto reader_func = [&](int idx) {
    rwlock.lock_shared();

    if (shared_data == 42) {
      active_readers.fetch_add(1, std::memory_order_relaxed);
      while (keep_reading.load(std::memory_order_relaxed)) {
        std::this_thread::yield();
      }
    }
    active_readers.fetch_sub(1, std::memory_order_relaxed);
    rwlock.unlock();
  };

  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(reader_func, i);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  ASSERT_EQ(active_readers.load(), num_threads);
  keep_reading.store(false);

  for (auto &t : threads) {
    t.join();
  }

  ASSERT_EQ(active_readers.load(), 0);
}

// Exclusive lock blocks readers. Also tests try_lock for the writer spinning
// until acquired.
TEST(RwLockTest, TestExclusiveLockBlocksReaders) {
  rwlock::RWLock lock;
  int shared_data = 42;
  std::atomic<int> active_readers{0};
  std::atomic<bool> keep_reading{true};
  const int first_readers = 3;
  const int second_readers = 2;
  bool writer_blocked = false;

  auto reader_func = [&](bool first_round) {
    ASSERT_EQ(lock.lock_shared(), 0);
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
    lock.unlock();
  };

  auto writer_func = [&]() {
    int start = active_readers.load(std::memory_order_relaxed);
    if (start > 0) {
      writer_blocked = true;
    }
    int rc;
    // Spin with try_lock until we get it
    while ((rc = lock.try_lock()) == EBUSY) {
      std::this_thread::yield();
    }
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(active_readers.load(), 0);
    ++shared_data;
    lock.unlock();
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

TEST(RwLockTest, TestMultipleWriters) {
  rwlock::RWLock lock;
  std::atomic<int> sim_writers{0}, max_writers{0};
  const int num_threads = 5;

  auto writer_func = [&]() {
    ASSERT_EQ(lock.lock(), 0);
    int c = sim_writers.fetch_add(1, std::memory_order_acq_rel) + 1;
    int m = max_writers.load(std::memory_order_relaxed);
    while (c > m) {
      max_writers.compare_exchange_weak(m, c);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sim_writers.fetch_sub(1, std::memory_order_acq_rel);
    lock.unlock();
  };

  std::vector<std::thread> threads(num_threads);
  for (auto &t : threads) {
    t = std::thread(writer_func);
  }
  for (auto &t : threads) {
    t.join();
  }
  EXPECT_EQ(max_writers.load(), 1);
}
