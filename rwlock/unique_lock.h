#pragma once

#include "rwlock/rw_lock.h"
#include <mutex>

namespace rwlock {

// Move‐only write‐lock guard (like std::unique_lock on a std::mutex).
class UniqueLock {
public:
  explicit UniqueLock(RWLock &rwlock) : lock_(&rwlock), owns_lock_(false) {
    int rc = lock_->lock();
    if (rc != 0) {
      lock_ = nullptr;
      throw std::system_error(rc, std::generic_category(),
                              "UniqueLock: Failed to acquire write lock");
    }
    owns_lock_ = true;
  }

  UniqueLock(RWLock &rwlock, std::try_to_lock_t)
      : lock_(&rwlock), owns_lock_(false) {
    int rc = lock_->try_lock();
    if (rc == 0) {
      owns_lock_ = true;
    } else if (rc == EBUSY) {
      // Lock is held by another thread.
      owns_lock_ = false;
    } else {
      // Other error code (EDEADLK, EINVAL, etc.).
      lock_ = nullptr;
      throw std::system_error(rc, std::generic_category(),
                              "UniqueLock: try_lock failed");
    }
  }

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

  // Returns true if we currently own an exclusive lock
  bool owns_lock() const noexcept { return owns_lock_; }

private:
  RWLock *lock_;
  bool owns_lock_;
};

} // namespace rwlock