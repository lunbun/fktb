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
