C++11 implementation of std::unique_lock and std::shared_lock using POSIX rwlock.

TODO: Implement the below methods

lock
	locks the associated mutex
try_lock
	tries to lock the associated mutex
try_lock_for
	tries to lock the associated mutex, for the specified duration
try_lock_until
	tries to lock the associated mutex, until a specified time point
unlock
	unlocks the associated mutex
