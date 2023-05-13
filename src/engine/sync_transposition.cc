#include "sync_transposition.h"

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



SpinLockGuard::SpinLockGuard(SpinLock &lock) : lock_(lock) {
    this->lock_.lock();
}

SpinLockGuard::~SpinLockGuard() {
    this->lock_.unlock();
}



SynchronizedTranspositionTable::SynchronizedTranspositionTable(uint32_t size) : lock_(), table_(size) { }