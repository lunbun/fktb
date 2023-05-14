#pragma once

#include <optional>
#include <cstdint>

#include "board.h"
#include "piece.h"
#include "transposition.h"

struct SearchNode {
    int32_t score;
    uint32_t nodeCount;
    uint32_t transpositionHits;

    SearchNode() = delete;
};

struct FixedDepthSearchResult {
    std::optional<Move> move;
    int32_t score;
    uint32_t nodeCount;
    uint32_t transpositionHits;

    FixedDepthSearchResult() = delete;
};

class FixedDepthSearcher {
public:
    FixedDepthSearcher(const Board &board, int32_t depth);
    FixedDepthSearcher(const Board &board, int32_t depth, FixedDepthSearcher *previousIteration);

    [[nodiscard]] FixedDepthSearchResult searchRoot();

private:
    Board board_;
    int32_t depth_;
    MoveListStack moves_;
    TranspositionTable table_;

    // Used for iterative deepening
    FixedDepthSearcher *previousIteration_;

    [[nodiscard]] SearchNode search(int32_t depth, int32_t alpha, int32_t beta);
};
