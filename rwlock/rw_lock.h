#pragma once

#include <cerrno>
#include <system_error>

#include <pthread.h>

namespace rwlock {

// Thin RAII wrapper for a POSIX pthread read-write lock.
class RWLock {
public:
  RWLock() {
    int rc = pthread_rwlock_init(&rwlock_, nullptr);
    if (rc != 0) {
      throw std::system_error(rc, std::generic_category(),
                              "Failed to initialize pthread_rwlock_t");
    }
  }

  ~RWLock() { pthread_rwlock_destroy(&rwlock_); }

  // Acquire write lock (exclusive). Blocking.
  int lock() {
    // If rc == EDEADLK, current thread already owns the read-write lock for
    // writing.
    // The lock was not acquired. Do not call unlock().
    return pthread_rwlock_wrlock(&rwlock_);
  }

  // Acquire write lock (exclusive). Non-blocking.
  int try_lock() {
    // If rc == EDEADLK, current thread already owns the read-write lock for
    // writing.
    // If rc == EBUSY, lock could not be acquired because it was already locked
    // for reading or writing.
    // The lock was not acquired. Do not call unlock().
    return pthread_rwlock_trywrlock(&rwlock_);
  }

  // Acquire read lock (shared). Blocking.
  int lock_shared() {
    // If rc == EDEADLK, current thread already owns the read-write lock for
    // writing.
    // If rc == EAGAIN, read lock could not be acquired because the maximum
    // number of read locks for rwlock has been exceeded.
    // The lock was not acquired. Do not call unlock().
    return pthread_rwlock_rdlock(&rwlock_);
  }

  // Acquire read lock (shared). Non-blocking.
  int try_lock_shared() {
    // If rc == EDEADLK, current thread already owns the read-write lock for
    // writing.
    // If rc == EAGAIN, read lock could not be acquired because the maximum
    // number of read locks for rwlock has been exceeded.
    // If rc == EBUSY, lock could not be acquired because a writer holds the
    // lock or a writer with the appropriate priority was blocked on it.
    // The lock was not acquired. Do not call unlock().
    return pthread_rwlock_tryrdlock(&rwlock_);
  }

  // Unlock from either a read or a write lock.
  void unlock() {
    int rc = pthread_rwlock_unlock(&rwlock_);
    if (rc != 0) {
      // Tried to unlock when caller didn't hold the lock. UB in POSIX.
      throw std::system_error(rc, std::generic_category(),
                              "Failed to unlock pthread_rwlock_t");
    }
  }

private:
  pthread_rwlock_t rwlock_;

  // Non-copyable, non-assignable
  RWLock(const RWLock &) = delete;
  RWLock &operator=(const RWLock &) = delete;
};

} // namespace rwlock