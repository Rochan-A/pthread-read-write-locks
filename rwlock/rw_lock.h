#pragma once

#include <cerrno>
#include <system_error>

#include <pthread.h>

namespace rwlock {

// Thin RAII wrapper for a POSIX pthread read-write lock.
class RWLock {
public:
  // Initialize the pthread read-write lock. Does not acquire it.
  // Throws system error if init fails.
  // Do not use directly. For use with unique or shared lock.
  RWLock() {
    int rc = pthread_rwlock_init(&rwlock_, nullptr);
    if (rc != 0) {
      throw std::system_error(rc, std::generic_category(),
                              "Failed to initialize pthread_rwlock_t");
    }
  }

  ~RWLock() { pthread_rwlock_destroy(&rwlock_); }

  // Acquire write lock (exclusive). Blocking.
  // If rc == EDEADLK, current thread already owns the read-write lock. If the
  // lock was not acquired, do not call unlock().
  const int lock() { return pthread_rwlock_wrlock(&rwlock_); }

  // Acquire write lock (exclusive). Non-blocking.
  // If rc == EDEADLK, current thread already owns the read-write lock for
  // writing.
  // If rc == EBUSY, lock could not be acquired because it was already locked
  // for reading or writing. If the lock was not acquired, do not call unlock().
  const int try_lock() { return pthread_rwlock_trywrlock(&rwlock_); }

  // Acquire read lock (shared). Blocking.
  // If rc == EDEADLK, current thread already owns the read-write lock for
  // writing.
  // If rc == EAGAIN, read lock could not be acquired because the maximum
  // number of read locks for rwlock has been exceeded. If the lock was not
  // acquired, do not call unlock().
  const int lock_shared() { return pthread_rwlock_rdlock(&rwlock_); }

  // Acquire read lock (shared). Non-blocking.
  // If rc == EDEADLK, current thread already owns the read-write lock for
  // writing.
  // If rc == EAGAIN, read lock could not be acquired because the maximum
  // number of read locks for rwlock has been exceeded.
  // If rc == EBUSY, lock could not be acquired because a writer holds the
  // lock or a writer with the appropriate priority was blocked on it.
  // If the lock was not acquired, do not call unlock().
  const int try_lock_shared() { return pthread_rwlock_tryrdlock(&rwlock_); }

  // Unlock from either a read or a write lock.
  const bool unlock() {
    int rc = pthread_rwlock_unlock(&rwlock_);
    if (rc == EPERM) {
      // Tried to unlock when caller didn't hold the lock. UB in POSIX.
      return false;
    } else if (rc == EINVAL) {
      // rwlock does not refer to an initialized read-write lock object. Should
      // not be possible.
      throw std::system_error(rc, std::generic_category(),
                              "Failed to unlock pthread_rwlock_t");
    }
    return true;
  }

  // Returns underlying pthread read-write lock.
  pthread_rwlock_t native_handle() { return rwlock_; }

  // Disallow move.
  RWLock(RWLock &&other) = delete;
  RWLock &operator=(RWLock &&other) = delete;

  // Disallow copy.
  RWLock(const RWLock &) = delete;
  RWLock &operator=(const RWLock &) = delete;

private:
  pthread_rwlock_t rwlock_;
};

} // namespace rwlock