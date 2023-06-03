#pragma once

#include <cstdint>
#include <atomic>
#include <chrono>

// Stores information like node count and transposition hits about the current search.
// This class is thread safe.
class SearchStatistics {
public:
    SearchStatistics() : nodeCount_(0), transpositionHits_(0), start_() {
        this->start_ = std::chrono::steady_clock::now();
    }

    void incrementNodeCount() {
        this->nodeCount_.fetch_add(1, std::memory_order_relaxed);
    }
    void incrementTranspositionHits() {
        this->transpositionHits_.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] uint64_t nodeCount() const {
        return this->nodeCount_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] uint64_t transpositionHits() const {
        return this->transpositionHits_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::chrono::milliseconds elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - this->start_);
    }

private:
    std::chrono::steady_clock::time_point start_;
    std::atomic<uint64_t> nodeCount_;
    std::atomic<uint64_t> transpositionHits_;
};
