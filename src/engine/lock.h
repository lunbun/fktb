#pragma once

#include <atomic>

class SpinLock {
public:
    SpinLock();

    void lock();
    void unlock();

private:
    std::atomic<bool> lock_;
};

class SpinLockGuard {
public:
    explicit SpinLockGuard(SpinLock &lock);
    ~SpinLockGuard();

private:
    SpinLock &lock_;
};
