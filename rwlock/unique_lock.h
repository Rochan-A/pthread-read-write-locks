#pragma once

#include "rwlock/rw_lock.h"
#include <mutex>
#include <thread>

namespace rwlock {

// Move‐only write‐lock guard (like std::unique_lock on a std::mutex). Takes
// exclusive ownership of RWLock.
class UniqueLock {
public:
  explicit UniqueLock(RWLock &rwlock) : lock_(&rwlock), owns_lock_(false) {
    lock();
  }

  // Take ownership of the rwlock (blocking).
  void lock() {
    int rc = lock_->lock();
    if (rc == EINVAL || rc == EDEADLK) {
      lock_ = nullptr;
      throw std::system_error(rc, std::generic_category(),
                              "Failed to lock() UniqueLock.");
    }
    owns_lock_ = true;
  }

  // Tries to take ownership of the rwlock (non-blocking).
  bool try_lock() {
    int rc = lock_->try_lock();
    if (rc == 0) {
      owns_lock_ = true;
    } else if (rc == EINVAL) {
      lock_ = nullptr;
      throw std::system_error(rc, std::generic_category(),
                              "Failed to try_lock() UniqueLock.");
    }
    // rc == EBUSY || rc == EDEADLK, Lock is held.
    return owns_lock_;
  }

  // Tries to take ownership of the rwlock, returns if lock has been
  // unavailable for the specified time duration.
  template <class Rep, class Period>
  bool
  try_lock_for(const std::chrono::duration<Rep, Period> &timeout_duration) {
    return try_lock_until(std::chrono::steady_clock::now() + timeout_duration);
  }

  // Tries to take ownership of the rwlock, returns if the mutex has been
  // unavailable until specified time point has been reached.
  template <class Clock, class Duration>
  bool
  try_lock_until(const std::chrono::time_point<Clock, Duration> &timeout_time) {
    using namespace std::chrono;
    while (Clock::now() < timeout_time) {
      if (try_lock()) {
        return true;
      }
      std::this_thread::sleep_for(0.01ms);
    }
    return false; // Timed out
  }

  // Releases ownership of the rwlock.
  void unlock() {
    if (owns_lock_ && lock_->unlock()) {
      owns_lock_ = false;
    }
  }

  // Swaps state with another unique lock.
  void swap(UniqueLock &other) {
    std::swap(lock_, other.lock_);
    std::swap(owns_lock_, other.owns_lock_);
  }

  // Disassociates the rwlock without unlocking (i.e., releasing
  // ownership of) it.
  RWLock *release() {
    RWLock *temp = lock_;
    lock_ = nullptr;
    owns_lock_ = false;
    return temp;
  }

  // Returns a pointer to the associated mutex
  RWLock *rwlock() { return lock_; }

  // Returns true if we currently own an exclusive lock
  bool owns_lock() const noexcept { return owns_lock_; }

  ~UniqueLock() {
    if (owns_lock_) {
      lock_->unlock();
    }
  }

  UniqueLock(UniqueLock &&other) noexcept
      : lock_(other.lock_), owns_lock_(other.owns_lock_) {
    other.lock_ = nullptr;
    other.owns_lock_ = false;
  }

  UniqueLock &operator=(UniqueLock &&other) noexcept {
    if (this != &other) {
      // If we own a lock, unlock before overwriting
      if (owns_lock_) {
        lock_->unlock();
      }
      lock_ = other.lock_;
      owns_lock_ = other.owns_lock_;

      other.lock_ = nullptr;
      other.owns_lock_ = false;
    }
    return *this;
  }

  UniqueLock(const UniqueLock &) = delete;
  UniqueLock &operator=(const UniqueLock &) = delete;

private:
  RWLock *lock_;
  bool owns_lock_;
};

} // namespace rwlock