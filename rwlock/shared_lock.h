#pragma once

#include "rwlock/rw_lock.h"
#include <mutex>
#include <thread>

namespace rwlock {

// Move‐only read‐lock guard (like std::shared_lock). Takes shared ownership of
// RWLock, if possible.
class SharedLock {
public:
  explicit SharedLock(RWLock &rwlock) : lock_(&rwlock), owns_lock_(false) {
    lock();
  }

  // Take shared ownership of the rwlock (blocking).
  void lock() {
    int rc = lock_->lock_shared(); // blocking read lock
    if (rc == EINVAL || rc == EDEADLK || rc == EAGAIN) {
      lock_ = nullptr;
      throw std::system_error(rc, std::generic_category(),
                              "Failed to lock() SharedLock.");
    }
    owns_lock_ = true;
  }

  // Tries to take shared ownership of the rwlock (non-blocking).
  bool try_lock() {
    int rc = lock_->try_lock();
    if (rc == 0) {
      owns_lock_ = true;
    } else if (rc == EINVAL) {
      lock_ = nullptr;
      throw std::system_error(rc, std::generic_category(),
                              "Failed to try_lock() SharedLock.");
    }
    // rc == EBUSY || rc == EDEADLK || rc == EAGAIN, Lock is held.
    return owns_lock_;
  }

  // Tries to take shared ownership of the rwlock, returns if lock has been
  // unavailable for the specified time duration.
  template <class Rep, class Period>
  bool
  try_lock_for(const std::chrono::duration<Rep, Period> &timeout_duration) {
    return try_lock_until(std::chrono::steady_clock::now() + timeout_duration);
  }

  // Tries to take shared ownership of the rwlock, returns if the mutex has been
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

  // Swaps state with another shared lock.
  void swap(SharedLock &other) {
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

  // Returns true if we currently own the shared lock
  bool owns_lock() const noexcept { return owns_lock_; }

  ~SharedLock() {
    if (owns_lock_) {
      lock_->unlock();
    }
  }

  SharedLock(RWLock &rwlock, std::try_to_lock_t)
      : lock_(&rwlock), owns_lock_(false) {
    int rc = lock_->try_lock_shared();
    if (rc == 0) {
      owns_lock_ = true;
    } else if (rc == EBUSY || rc == EAGAIN) {
      // Lock is busy or read-limit reached.
      owns_lock_ = false;
    } else {
      // Other error code (EDEADLK, EINVAL, etc.).
      lock_ = nullptr;
      throw std::system_error(rc, std::generic_category(),
                              "SharedLock: try_lock_shared failed");
    }
  }

  SharedLock(SharedLock &&other) noexcept
      : lock_(other.lock_), owns_lock_(other.owns_lock_) {
    other.lock_ = nullptr;
    other.owns_lock_ = false;
  }

  SharedLock &operator=(SharedLock &&other) noexcept {
    if (this != &other) {
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

  SharedLock(const SharedLock &) = delete;
  SharedLock &operator=(const SharedLock &) = delete;

private:
  RWLock *lock_;
  bool owns_lock_;
};

} // namespace rwlock