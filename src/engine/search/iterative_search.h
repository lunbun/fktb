#pragma once

#include <cstdint>
#include <mutex>
#include <memory>
#include <vector>
#include <chrono>
#include <functional>

#include "debug_info.h"
#include "fixed_search.h"
#include "engine/inline.h"
#include "engine/board/piece.h"
#include "engine/hash/transposition.h"

// Note: In IterativeSearcher, nodeCount, transpositionHits, and elapsed are the amount of nodes/hits/elapsed time since
// the start of the entire iterative search, not just the current iteration.
struct SearchResult {
    INLINE static SearchResult invalid() { return { }; }

    uint16_t depth;
    std::vector<Move> bestLine;
    int32_t score;
    uint64_t nodeCount;
    uint64_t transpositionHits;
    std::chrono::milliseconds elapsed;

    SearchResult(); // Creates an invalid search result.
    SearchResult(uint16_t depth, SearchLine line, const SearchDebugInfo &debugInfo);

    [[nodiscard]] INLINE bool isValid() const { return !this->bestLine.empty(); }
};

using IterationCallback = std::function<void(const SearchResult &result)>;

class IterativeSearcher {
public:
    explicit IterativeSearcher(uint32_t threadCount);
    ~IterativeSearcher();

    void addIterationCallback(IterationCallback callback);

    void start(const Board &board);
    SearchResult stop();

private:
    class SearchThread;

    std::vector<std::unique_ptr<SearchThread>> threads_;
    std::mutex mutex_;

    std::vector<IterationCallback> callbacks_;
    SearchResult result_;
    std::unique_ptr<TranspositionTable> table_;
    std::unique_ptr<SearchDebugInfo> debugInfo_;

    void notifyCallbacks(const SearchResult &result);
    void receiveResultFromThread(const SearchResult &result);
};
