#include "search.h"

#include <thread>
#include <vector>
#include <optional>
#include <mutex>
#include <iostream>
#include <stdexcept>

#include "move.h"
#include "movegen.h"
#include "movesort.h"
#include "sync_transposition.h"

ManagerIterationData::ManagerIterationData() : moves(1, 64), moveQueueIndex(0), movesSearched(0),
                                               transpositionTable(65536), bestScore(-INT32_MAX),
                                               bestMove(std::nullopt), totalNodesSearched(0),
                                               transpositionHits(0) { }

ManagerIterativeSearchData::ManagerIterativeSearchData(const Board &board, int32_t depth) : board(board.copy()),
                                                                                            depth(depth) { }

SearchThreadManager::SearchThreadManager(uint32_t threads) : mutex_(), threads_(), previousIterationData_(nullptr),
                                                             iterationData_(nullptr) {
    this->threads_.reserve(threads);
    for (uint32_t i = 0; i < threads; ++i) {
        this->threads_.emplace_back(std::make_unique<SearchThread>(this, i));
    }
}



void SearchThreadManager::startIterativeSearch(const Board &board) {
    std::lock_guard<std::mutex> lock(this->mutex_);

    this->iterativeSearchData_ = std::make_unique<ManagerIterativeSearchData>(board, 2);

    this->previousIterationData_ = nullptr;
    this->iterationData_ = nullptr;

    this->startSearchIteration(board, 2);
}



bool SearchThreadManager::isIterationComplete() const {
    return this->iterationData_ == nullptr || this->iterationData_->movesSearched >= this->iterationData_->moves.size();
}

void SearchThreadManager::startSearchIteration(const Board &board, int32_t depth) {
    if (!this->isIterationComplete()) {
        throw std::runtime_error("Already searching");
    }

    if (depth < 2) {
        throw std::runtime_error("Depth must be at least 2 to use multi-threaded search");
    }

    // Reset the state of the search
    this->previousIterationData_ = std::move(this->iterationData_);
    this->iterationData_ = std::make_unique<ManagerIterationData>();

    this->iterationData_->moves.push();

    MoveGenerator::generate(board, this->iterationData_->moves);
    MoveSort::sort(this->iterationData_->moves);

    for (const auto &thread : this->threads_) {
        thread->initSearch(board, depth - 1);
    }
}

void SearchThreadManager::nextSearchIteration() {
    this->iterativeSearchData_->depth++;

    this->startSearchIteration(this->iterativeSearchData_->board, this->iterativeSearchData_->depth);

    IterativeSearchResult result = this->getIterativeSearchResult();
    std::cout << "depth " << result.depth << " " << result.node.move->debugName() << std::endl;
}

IterativeSearchResult SearchThreadManager::getIterativeSearchResult() const {
    // Use the data of the last iteration
    if (this->previousIterationData_ == nullptr) {
        throw std::runtime_error("No result available");
    }

    return {
        this->iterativeSearchData_->depth - 1,
        {
            this->previousIterationData_->bestMove,
            this->previousIterationData_->bestScore,
            this->previousIterationData_->totalNodesSearched,
            this->previousIterationData_->transpositionHits
        }
    };
}



std::optional<Move> SearchThreadManager::pollNextMove() {
    std::lock_guard<std::mutex> lock(this->mutex_);

    if (this->isIterationComplete() || this->iterationData_->moveQueueIndex >= this->iterationData_->moves.size()) {
        return std::nullopt;
    }

    return this->iterationData_->moves.unsafeAt(this->iterationData_->moveQueueIndex++);
}

void SearchThreadManager::receiveResult(SearchThread *thread, Move move, SearchNode node) {
    std::lock_guard<std::mutex> lock(this->mutex_);

    this->iterationData_->movesSearched++;
    this->iterationData_->totalNodesSearched += node.count;
    this->iterationData_->transpositionHits += node.transpositionHits;

    int32_t score = -node.score;
    if (score > this->iterationData_->bestScore) {
        this->iterationData_->bestScore = score;
        this->iterationData_->bestMove = move;
    }

    if (this->isIterationComplete()) {
        this->nextSearchIteration();
    }
}



SearchThreadData::SearchThreadData(const Board &board, int32_t depth) : board(board.copy()), moves(depth, 64 * depth),
                                                                        depth(depth) { }

SearchThread::SearchThread(SearchThreadManager *manager, uint32_t id) : manager_(manager), id_(id), data_(nullptr) {
    this->thread_ = std::thread(&SearchThread::loop, this);
    this->thread_.detach();
}

void SearchThread::initSearch(const Board &board, int32_t depth) {
    this->data_ = std::make_unique<SearchThreadData>(board, depth);
}

void SearchThread::loop() {
    while (true) {
        std::optional<Move> move = this->manager_->pollNextMove();
        if (!move.has_value()) {
            std::this_thread::yield();
            continue;
        }

        SearchNode node = this->search(move.value());

        this->manager_->receiveResult(this, move.value(), node);
    }
}

SearchNode SearchThread::search(Move move) const {
    Board &board = this->data_->board;
    int32_t depth = this->data_->depth;

    board.makeMove(move);

    SearchNode node = this->search(depth, -INT32_MAX, INT32_MAX);

    board.unmakeMove(move);

    return node;
}



int32_t evaluate(const Board &board) {
    return board.material(board.turn()) - board.material(~board.turn());
}

SearchNode SearchThread::search(int32_t depth, int32_t alpha, int32_t beta) const {
    Board &board = this->data_->board;
    MoveListStack &moves = this->data_->moves;
    SynchronizedTranspositionTable &table = this->manager_->transpositionTable();

    if (depth == 0) {
        return { std::nullopt, evaluate(board), 1, 0 };
    }

    // Transposition table lookup
    {
        SpinLockGuard lock(table.lock());

        TranspositionTable::Entry *entry = table.table().load(board.hash());
        if (entry != nullptr && entry->depth >= depth) {
            if (entry->flag == TranspositionTable::Flag::Exact) {
                return { entry->bestMove, entry->bestScore, 0, 1 };
            } else if (entry->flag == TranspositionTable::Flag::LowerBound) {
                alpha = std::max(alpha, entry->bestScore);
            } else if (entry->flag == TranspositionTable::Flag::UpperBound) {
                beta = std::min(beta, entry->bestScore);
            }

            if (alpha >= beta) {
                return { entry->bestMove, entry->bestScore, 0, 1 };
            }
        }
    }

    int32_t originalAlpha = alpha;

    // Move search
    moves.push();
    MoveGenerator::generate(board, moves);

    if (moves.size() == 0) {
        moves.pop();
        // TODO: Stalemate
        return { std::nullopt, -INT32_MAX + depth, 1, 0 };
    }

    ManagerIterationData *previousIterationData = this->manager_->previousIterationData();
    if (previousIterationData != nullptr) {
        MoveSort::sort(board, moves, previousIterationData->transpositionTable);
    } else {
        MoveSort::sort(moves);
    }

    int32_t bestScore = -INT32_MAX;
    std::optional<Move> bestMove;
    uint32_t count = 0;
    uint32_t transpositionHits = 0;

    for (uint32_t i = 0; i < moves.size(); ++i) {
        Move move = moves.unsafeAt(i);

        board.makeMove(move);

        SearchNode node = search(depth - 1, -beta, -alpha);
        count += node.count;
        transpositionHits += node.transpositionHits;

        int32_t score = -node.score;

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }

        board.unmakeMove(move);

        if (alpha > beta) {
            break;
        }
        alpha = std::max(alpha, score);
    }

    moves.pop();

    // Transposition table store
    TranspositionTable::Flag flag;
    if (bestScore <= originalAlpha) {
        flag = TranspositionTable::Flag::UpperBound;
    } else if (bestScore >= beta) {
        flag = TranspositionTable::Flag::LowerBound;
    } else {
        flag = TranspositionTable::Flag::Exact;
    }
    {
        SpinLockGuard lock(table.lock());
        table.table().store(board.hash(), { true, board.hash().hash(), depth, flag, bestMove, bestScore });
    }

    return { bestMove.value(), bestScore, count, transpositionHits };
}
