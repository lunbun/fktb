#pragma once

#include <optional>
#include <cstdint>
#include <vector>

#include "board.h"
#include "piece.h"
#include "move.h"
#include "move_queue.h"
#include "transposition.h"

struct SearchNode {
    static SearchNode invalid();

    int32_t score;
    uint32_t nodeCount;
    uint32_t transpositionHits;

    SearchNode() = delete;
};

struct SearchRootNode {
    static SearchRootNode invalid();

    Move move;
    int32_t score;
    uint32_t nodeCount;
    uint32_t transpositionHits;

    SearchRootNode() = delete;
};

struct SearchResult {
    static SearchResult invalid();

    bool isValid;
    uint16_t depth;
    std::vector<Move> bestLine;
    int32_t score;
    uint32_t nodeCount;
    uint32_t transpositionHits;

    SearchResult() = delete;
};

class FixedDepthSearcher {
public:
    FixedDepthSearcher(const Board &board, uint16_t depth, TranspositionTable &table);
    FixedDepthSearcher(const Board &board, uint16_t depth, TranspositionTable &table, ReadonlyTranspositionTable *previousTable);

    [[nodiscard]] SearchResult search();

    // Tells the searcher to stop searching as soon as possible. This is not guaranteed to stop the search immediately,
    // but it will stop the search as soon as possible. Nodes returned from the search will be invalid, and the
    // transposition table may be corrupted after this function is called.
    //
    // Intended to be called from a different thread (otherwise it would be impossible to call this as the search would
    // be blocking the thread).
    void halt();

private:
    volatile bool isHalted_ = false;

    Board board_;
    uint16_t depth_;
    MovePriorityQueueStack moves_;
    TranspositionTable &table_;

    // Previous iteration table, used for iterative deepening.
    ReadonlyTranspositionTable *previousTable_;

    template<Color Turn>
    [[nodiscard]] SearchRootNode searchRoot();

    template<Color Turn>
    [[nodiscard]] SearchNode searchQuiesce(int32_t alpha, int32_t beta);
    template<Color Turn>
    [[nodiscard]] SearchNode searchNoTransposition(Move &bestMove, uint16_t depth, int32_t &alpha, int32_t beta);
    template<Color Turn>
    [[nodiscard]] SearchNode search(uint16_t depth, int32_t alpha, int32_t beta);
};
