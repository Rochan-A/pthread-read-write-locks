#pragma once

#include "rwlock/rw_lock.h"
#include <mutex>

namespace rwlock {

// Move‐only read‐lock guard (like std::shared_lock).
class SharedLock {
public:
  explicit SharedLock(RWLock &rwlock) : lock_(&rwlock), owns_lock_(false) {
    int rc = lock_->lock_shared(); // blocking read lock
    if (rc != 0) {
      lock_ = nullptr;
      throw std::system_error(rc, std::generic_category(),
                              "SharedLock: failed to acquire read lock");
    }
    owns_lock_ = true;
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

  ~SharedLock() {
    if (owns_lock_) {
      lock_->unlock();
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

  // Returns true if we currently own a shared lock.
  bool owns_lock() const noexcept { return owns_lock_; }

private:
  RWLock *lock_;
  bool owns_lock_;
};

} // namespace rwlock