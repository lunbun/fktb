#pragma once

#include <optional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "fixed_search.h"
#include "inline.h"

using IterationCallback = std::function<void(const SearchResult &result)>;

// Search starts immediately after construction, and continues until the object is destroyed.
class IterativeSearchThread {
private:
    struct SearchData {
        uint16_t depth;
        SearchResult result;
        Board board;
        std::unique_ptr<ReadonlyTranspositionTable> previousTable;
        TranspositionTable table;
        std::unique_ptr<FixedDepthSearcher> currentIteration;

        explicit SearchData(Board board);
    };

public:
    explicit IterativeSearchThread();

    void addIterationCallback(IterationCallback callback);

    void start(const Board &board);
    SearchResult stop();

    [[nodiscard]] INLINE bool isSearching() const { return this->data_ != nullptr; }
    [[nodiscard]] INLINE SearchData &data() const { return *this->data_; }

private:
    std::thread thread_;
    std::mutex dataMutex_;
    std::mutex searchMutex_;
    std::condition_variable isSearchingCondition_;
    std::vector<IterationCallback> iterationCallbacks_;

    std::unique_ptr<SearchData> data_;

    [[noreturn]] void loop();

    void notifyCallbacks(const SearchResult &result);

    void awaitSearchStart();
};
