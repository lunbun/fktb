#pragma once

#include <optional>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "board.h"
#include "piece.h"
#include "sync_transposition.h"

struct SearchNode {
    std::optional<Move> move;
    int32_t score;
    uint32_t count;
    uint32_t transpositionHits;

    SearchNode() = delete;
};

struct IterativeSearchResult {
    int32_t depth;
    SearchNode node;

    IterativeSearchResult() = delete;
};

class SearchThreadManager;
class SearchThread;

struct ManagerIterativeSearchData {
    Board board;
    int32_t depth;

    ManagerIterativeSearchData(const Board &board, int32_t depth);
};

struct ManagerIterationData {
    MoveListStack moves;
    uint32_t moveQueueIndex;
    uint32_t movesSearched;

    SynchronizedTranspositionTable transpositionTable;

    int32_t bestScore;
    std::optional<Move> bestMove;
    uint32_t totalNodesSearched;
    uint32_t transpositionHits;

    ManagerIterationData();
};

class SearchThreadManager {
public:
    explicit SearchThreadManager(uint32_t threads);

    void startIterativeSearch(const Board &board);

private:
    std::mutex mutex_;
    std::vector<std::unique_ptr<SearchThread>> threads_;

    std::unique_ptr<ManagerIterativeSearchData> iterativeSearchData_;
    std::unique_ptr<ManagerIterationData> previousIterationData_;
    std::unique_ptr<ManagerIterationData> iterationData_;



    // Responsibility of the caller to lock the mutex before calling this functions.
    [[nodiscard]] bool isIterationComplete() const;

    // Responsibility of the caller to lock the mutex before calling these functions.
    void startSearchIteration(const Board &board, int32_t depth);
    void nextSearchIteration();
    [[nodiscard]] IterativeSearchResult getIterativeSearchResult() const;



    friend class SearchThread;

    [[nodiscard]] inline ManagerIterationData *previousIterationData() const {
        return this->previousIterationData_.get();
    }
    [[nodiscard]] inline SynchronizedTranspositionTable &transpositionTable() const {
        return this->iterationData_->transpositionTable;
    }

    // Returns the next move to search, or std::nullopt if there are no more moves to search.
    // Locks the mutex for you.
    [[nodiscard]] std::optional<Move> pollNextMove();

    // Receives the result of a search from a thread.
    // Locks the mutex for you.
    void receiveResult(SearchThread *thread, Move move, SearchNode node);
};

struct SearchThreadData {
    Board board;
    MoveListStack moves;
    int32_t depth;

    SearchThreadData(const Board &board, int32_t depth);
};

class SearchThread {
public:
    SearchThread(SearchThreadManager *manager, uint32_t id);

    [[nodiscard]] inline uint32_t id() const { return this->id_; }

    void initSearch(const Board &board, int32_t depth);

private:
    SearchThreadManager *manager_;
    uint32_t id_;

    std::thread thread_;

    std::unique_ptr<SearchThreadData> data_;

    [[noreturn]] void loop();

    [[nodiscard]] SearchNode search(Move move) const;
    [[nodiscard]] SearchNode search(int32_t depth, int32_t alpha, int32_t beta) const;
};
