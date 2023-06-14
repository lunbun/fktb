#pragma once

#include <cstdint>
#include <atomic>
#include <chrono>

#include "engine/inline.h"

// Stores information like node count and transposition hits about the current search.
// This class is thread safe.
class SearchStatistics {
public:
    INLINE SearchStatistics();

    INLINE void reset();

    INLINE void incrementNodeCount() { this->nodeCount_.fetch_add(1, std::memory_order_relaxed); }
    INLINE void incrementTranspositionHits() { this->transpositionHits_.fetch_add(1, std::memory_order_relaxed); }

    [[nodiscard]] INLINE uint64_t nodeCount() const { return this->nodeCount_.load(std::memory_order_relaxed); }
    [[nodiscard]] INLINE uint64_t transpositionHits() const { return this->transpositionHits_.load(std::memory_order_relaxed); }
    [[nodiscard]] INLINE std::chrono::milliseconds elapsed() const;

private:
    std::chrono::steady_clock::time_point start_;
    std::atomic<uint64_t> nodeCount_;
    std::atomic<uint64_t> transpositionHits_;
};

INLINE SearchStatistics::SearchStatistics() : nodeCount_(0), transpositionHits_(0), start_() {
    this->start_ = std::chrono::steady_clock::now();
}

INLINE void SearchStatistics::reset() {
    this->nodeCount_.store(0, std::memory_order_relaxed);
    this->transpositionHits_.store(0, std::memory_order_relaxed);
    this->start_ = std::chrono::steady_clock::now();
}

INLINE std::chrono::milliseconds SearchStatistics::elapsed() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - this->start_);
}
