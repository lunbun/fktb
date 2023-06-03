#include "iterative_search.h"

#include <optional>
#include <stdexcept>
#include <thread>
#include <algorithm>
#include <random>
#include <condition_variable>
#include <cassert>

#include "fixed_search.h"
#include "statistics.h"
#include "engine/move/movegen.h"

SearchResult::SearchResult() : depth(0), bestLine(), score(0), nodeCount(0), transpositionHits(0), elapsed() { }

SearchResult::SearchResult(uint16_t depth, SearchLine line, const SearchStatistics &stats) : elapsed() {
    this->depth = depth;
    this->bestLine = std::move(line.moves);
    this->score = line.score;
    this->nodeCount = stats.nodeCount();
    this->transpositionHits = stats.transpositionHits();
    this->elapsed = stats.elapsed();
}



struct SearchTask {
    Board board;
    uint16_t depth = 1;
    std::optional<RootMoveList> rootMoveOrder = std::nullopt;
    bool canUseHashMove = false;
    TranspositionTable &table;
    SearchStatistics &stats;
    std::unique_ptr<FixedDepthSearcher> iteration = nullptr;

    SearchTask(const Board &board, TranspositionTable &table, SearchStatistics &stats) : board(board.copy()),
                                                                                            table(table),
                                                                                            stats(stats) { }
};



// TODO: Add a destructor to properly kill the thread
class IterativeSearcher::SearchThread {
public:
    explicit SearchThread(IterativeSearcher &manager);

    // Starts an iterative deepening search beginning from the given depth and root node move order.
    void start(std::unique_ptr<SearchTask> task);
    void stop();

    [[nodiscard]] INLINE bool isSearching() const { return this->task_ != nullptr; }

private:
    IterativeSearcher &manager_;

    std::thread thread_;

    // Task mutex is used for synchronizing access to task_.
    std::mutex taskMutex_;

    // Search mutex is locked when the searcher is searching, and unlocked when it is not. This allows the main thread
    // to wait for the searcher to finish by locking the search mutex.
    std::mutex searchMutex_;

    std::condition_variable isSearchingCondition_;

    std::unique_ptr<SearchTask> task_;

    void awaitTask();
    SearchResult searchIteration();

    [[noreturn]] void loop();
};



IterativeSearcher::SearchThread::SearchThread(IterativeSearcher &manager) : manager_(manager), taskMutex_(),
                                                                            searchMutex_(), isSearchingCondition_(),
                                                                            task_(nullptr) {
    this->thread_ = std::thread(&SearchThread::loop, this);
    this->thread_.detach();
}

void IterativeSearcher::SearchThread::start(std::unique_ptr<SearchTask> task) {
    std::lock_guard<std::mutex> lock(this->taskMutex_);

    if (this->isSearching()) {
        throw std::runtime_error("Already searching");
    }

    this->task_ = std::move(task);

    this->isSearchingCondition_.notify_all();
}

void IterativeSearcher::SearchThread::stop() {
    std::lock_guard<std::mutex> lock(this->taskMutex_);

    if (!this->isSearching()) {
        throw std::runtime_error("Not searching");
    }

    // Stop the current search
    this->task_->iteration->halt();

    // Wait for the search to stop by waiting for the search mutex to be released (will be released when the searcher
    // is done)
    std::lock_guard<std::mutex> searchLock(this->searchMutex_);

    this->task_ = nullptr;
}



void IterativeSearcher::SearchThread::awaitTask() {
    // Check if we already have a task
    bool hasTask;
    {
        std::lock_guard<std::mutex> lock(this->taskMutex_);
        hasTask = (this->task_ != nullptr);
    }

    if (hasTask) {
        return;
    }

    // Wait for a task to be assigned
    std::unique_lock<std::mutex> lock(this->taskMutex_);
    this->isSearchingCondition_.wait(lock, [this]() {
        return this->isSearching();
    });
}

SearchResult IterativeSearcher::SearchThread::searchIteration() {
    // Create a copy of parts of the task that we need, so that we are not holding the task mutex while searching.
    // Also create the FixedDepthSearcher here.
    uint16_t depth;
    SearchStatistics *stats;
    std::optional<RootMoveList> rootMoveOrder;
    FixedDepthSearcher *iteration;

    {
        std::lock_guard<std::mutex> lock(this->taskMutex_);

        SearchTask &task = *this->task_;

        depth = task.depth;
        stats = &task.stats;

        rootMoveOrder = task.rootMoveOrder;
        if (task.canUseHashMove) {
            // Load the hash move
            rootMoveOrder->loadHashMove(task.board, task.table);
        }

        // Create a new searcher
        task.iteration = std::make_unique<FixedDepthSearcher>(task.board, task.depth, task.table, task.stats);

        iteration = task.iteration.get();
    }

    // Search
    std::lock_guard<std::mutex> lock(this->searchMutex_);
    SearchLine line = iteration->search(rootMoveOrder.value());
    return { depth, std::move(line), *stats };
}

void IterativeSearcher::SearchThread::loop() {
    while (true) {
        this->awaitTask();

        SearchResult result = this->searchIteration();

        if (result.isValid()) {
            std::lock_guard<std::mutex> lock(this->taskMutex_);

            // Increment depth
            this->task_->depth++;

            // Notify manager that we have a result
            this->manager_.receiveResultFromThread(result);
        }
    }
}



IterativeSearcher::IterativeSearcher(uint32_t threadCount) : threads_(), mutex_(), callbacks_(),
                                                             result_(SearchResult::invalid()), table_(nullptr) {
    // Lazy SMP seems to be broken atm so only allow one thread
    assert(threadCount == 1);

    this->threads_.reserve(threadCount);
    for (uint32_t i = 0; i < threadCount; i++) {
        this->threads_.push_back(std::make_unique<SearchThread>(*this));
    }
}

IterativeSearcher::~IterativeSearcher() = default;

void IterativeSearcher::addIterationCallback(IterationCallback callback) {
    this->callbacks_.push_back(std::move(callback));
}

void IterativeSearcher::notifyCallbacks(const SearchResult &result) {
    for (const auto &callback : this->callbacks_) {
        callback(result);
    }
}

void IterativeSearcher::receiveResultFromThread(const SearchResult &result) {
    std::lock_guard<std::mutex> lock(this->mutex_);

    if (!result.isValid()) {
        return;
    }

    // Check if the result is deeper than the current result
    if (!this->result_.isValid() || result.depth > this->result_.depth) {
        this->result_ = result;
        this->notifyCallbacks(result);
    }
}

void IterativeSearcher::start(const Board &board) {
    std::lock_guard<std::mutex> lock(this->mutex_);

    this->result_ = SearchResult::invalid();
    this->table_ = std::make_unique<TranspositionTable>(4194304);
    this->stats_ = std::make_unique<SearchStatistics>();

    Board boardCopy = board.copy();
    RootMoveList rootMoves = MoveGeneration::generateLegalRoot(boardCopy);

    std::random_device device;
    std::mt19937 generator(device());

    for (uint32_t i = 0; i < this->threads_.size(); i++) {
        std::unique_ptr<SearchTask> task = std::make_unique<SearchTask>(board, *this->table_, *this->stats_);

        // Create a copy of the root moves so that we can modify it
        RootMoveList rootMoveOrder = rootMoves;

        if (i == 0) {
            // First thread is our "primary" thread, so it should not randomize anything and should use the hash move.
            rootMoveOrder = rootMoves;

            rootMoveOrder.score(board);
            rootMoveOrder.sort();

            task->depth = 1;

            // Only permit the first thread to use the hash move, since otherwise, multiple threads will be searching
            // the hash move at the same time, leading to lots of wasted search time (lots of time will be spent waiting
            // on the transposition table lock since many threads are trying to access the same entry since they are
            // searching the same tree).
            task->canUseHashMove = true;
        } else {
            // Other threads are helper threads, so they should randomize the root move order and start at a higher
            // depth.

            // Start at a higher depth for other threads.
            task->depth = (i / 2) + 5;
            task->canUseHashMove = false;
            if ((i % 7) == 0) {
                // Have every seventh thread have an insanely high depth just for fun
                task->depth = (i * 3) + 5;
            }

            // Move ordering
            rootMoveOrder = rootMoves;
            if ((i % 4) == 0) {
                // Have every fourth thread just have a completely random move order
                std::shuffle(rootMoveOrder.moves().begin(), rootMoveOrder.moves().end(), generator);
            } else {
                // Other threads have similar move order to the primary thread, but with some randomization
                rootMoveOrder.score(board);

                // Add a small random value to the score of each move
                int32_t range = (25 * static_cast<int32_t>(i));
                std::uniform_int_distribution<int32_t> distribution(-range, range);
                for (MoveEntry &entry : rootMoveOrder.moves()) {
                    entry.score += distribution(generator);
                }

                rootMoveOrder.sort();
            }
        }

        task->rootMoveOrder = std::move(rootMoveOrder);

        this->threads_[i]->start(std::move(task));
    }
}

SearchResult IterativeSearcher::stop() {
    std::lock_guard<std::mutex> lock(this->mutex_);

    for (const auto &thread : this->threads_) {
        thread->stop();
    }

    SearchResult result = std::move(this->result_);

    this->result_ = SearchResult::invalid();
    this->table_ = nullptr;
    this->stats_ = nullptr;

    return result;
}
