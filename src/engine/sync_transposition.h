#pragma once

#include <atomic>

#include "transposition.h"

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

class SynchronizedTranspositionTable {
public:
    explicit SynchronizedTranspositionTable(uint32_t size);

    [[nodiscard]] inline SpinLock &lock() { return this->lock_; }
    [[nodiscard]] inline TranspositionTable &table() { return this->table_; }

private:
    SpinLock lock_;
    TranspositionTable table_;
};
