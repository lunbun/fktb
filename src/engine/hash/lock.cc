#include "lock.h"

SpinLock::SpinLock() : lock_(false) { }

void SpinLock::lock() {
    for (;;) {
        if (!this->lock_.exchange(true, std::memory_order_acquire)) {
            break;
        }

        while (this->lock_.load(std::memory_order_relaxed)) {
            __builtin_ia32_pause();
        }
    }
}

void SpinLock::unlock() {
    this->lock_.store(false, std::memory_order_release);
}
