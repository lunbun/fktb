#include "iterative_search.h"

#include <optional>
#include <stdexcept>
#include <thread>
#include <condition_variable>

#include "fixed_search.h"

IterativeSearchThread::SearchData::SearchData(Board board) : depth(1), result(SearchResult::invalid()),
                                                             board(std::move(board)), previousTable(nullptr),
                                                             table(2097152), currentIteration(nullptr) { }

IterativeSearchThread::IterativeSearchThread() : dataMutex_(), searchMutex_(), isSearchingCondition_(),
                                                 iterationCallbacks_(), data_(nullptr) {
    this->thread_ = std::thread(&IterativeSearchThread::loop, this);
    this->thread_.detach();
}

void IterativeSearchThread::addIterationCallback(IterationCallback callback) {
    this->iterationCallbacks_.push_back(std::move(callback));
}

void IterativeSearchThread::notifyCallbacks(const SearchResult &result) {
    for (const IterationCallback &callback : this->iterationCallbacks_) {
        callback(result);
    }
}

void IterativeSearchThread::awaitSearchStart() {
    std::unique_lock<std::mutex> lock(this->dataMutex_);
    this->isSearchingCondition_.wait(lock, [this]() {
        return this->isSearching();
    });
}

void IterativeSearchThread::start(const Board &board) {
    std::lock_guard<std::mutex> lock(this->dataMutex_);

    if (this->isSearching()) {
        throw std::runtime_error("Already searching");
    }

    this->data_ = std::make_unique<SearchData>(board.copy());

    this->isSearchingCondition_.notify_all();
}

SearchResult IterativeSearchThread::stop() {
    std::lock_guard<std::mutex> lock(this->dataMutex_);

    if (!this->isSearching()) {
        throw std::runtime_error("Not searching");
    }

    SearchData &data = this->data();

    // Stop the current search
    data.currentIteration->halt();

    // Wait for the search to stop by waiting for the search mutex to be released (will be released when the searcher
    // is done)
    std::lock_guard<std::mutex> searchLock(this->searchMutex_);

    SearchResult result = std::move(data.result);

    this->data_ = nullptr;

    return result;
}

void IterativeSearchThread::loop() {
    while (true) {
        bool isSearching;
        {
            std::lock_guard<std::mutex> lock(this->dataMutex_);
            isSearching = this->isSearching();
        }

        if (!isSearching) {
            this->awaitSearchStart();
            continue;
        }

        FixedDepthSearcher *currentIteration;
        {
            std::lock_guard<std::mutex> lock(this->dataMutex_);
            SearchData &data = this->data();

            // If we have a previous iteration, make a readonly copy of the table and store it
            if (data.currentIteration != nullptr) {
                data.previousTable = std::make_unique<ReadonlyTranspositionTable>(data.table.readonlyCopy());
            }

            // Start a new search
            data.currentIteration = std::make_unique<FixedDepthSearcher>(data.board, data.depth, data.table,
                data.previousTable.get());

            currentIteration = data.currentIteration.get();
        }

        std::optional<SearchResult> result;
        {
            std::lock_guard<std::mutex> lock(this->searchMutex_);
            result = currentIteration->search();
        }

        if (result.has_value() && result->isValid) {
            std::lock_guard<std::mutex> lock(this->dataMutex_);
            SearchData &data = this->data();

            data.depth++;
            this->notifyCallbacks(result.value());
            data.result = std::move(result.value());
        }
    }
}
