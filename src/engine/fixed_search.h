#pragma once

#include <optional>
#include <cstdint>
#include <vector>

#include "board.h"
#include "piece.h"
#include "transposition.h"

struct SearchNode {
    static SearchNode invalid();

    std::optional<Move> move;
    int32_t score;
    uint32_t nodeCount;
    uint32_t transpositionHits;

    SearchNode() = delete;
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
    FixedDepthSearcher(const Board &board, uint16_t depth);
    FixedDepthSearcher(const Board &board, uint16_t depth, FixedDepthSearcher *previousIteration);

    [[nodiscard]] SearchResult searchRoot();

    [[nodiscard]] inline TranspositionTable &table() { return this->table_; }

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
    TranspositionTable table_;

    // Used for iterative deepening
    FixedDepthSearcher *previousIteration_;

    [[nodiscard]] SearchNode search(uint16_t depth, int32_t alpha, int32_t beta);
};
