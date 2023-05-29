#pragma once

#include <optional>
#include <cstdint>
#include <memory>
#include <vector>

#include "board.h"
#include "piece.h"
#include "move.h"
#include "move_score.h"
#include "move_list.h"
#include "transposition.h"
#include "debug_info.h"
#include "inline.h"

struct SearchRootNode {
    INLINE constexpr static SearchRootNode invalid() { return { Move::invalid(), 0 }; };

    Move move;
    int32_t score;

    SearchRootNode() = delete;
};

struct SearchLine {
    INLINE static SearchLine invalid() { return { {}, 0 }; };

    std::vector<Move> moves;
    int32_t score;

    SearchLine() = delete;

    [[nodiscard]] INLINE bool isValid() const { return !this->moves.empty(); }
};

class FixedDepthSearcher {
public:
    FixedDepthSearcher(const Board &board, uint16_t depth, TranspositionTable &table, SearchDebugInfo &debugInfo);

    [[nodiscard]] SearchLine search();

    // Allows control over the move ordering of the root node.
    // Note: if you want to use the hash move, you must add it to the move list yourself.
    [[nodiscard]] SearchLine search(RootMoveList moves);

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
    TranspositionTable &table_;
    SearchDebugInfo &debugInfo_;

    template<Color Turn>
    [[nodiscard]] SearchRootNode searchRoot(RootMoveList moves);

    template<Color Turn>
    [[nodiscard]] int32_t searchQuiesce(int32_t alpha, int32_t beta);
    template<Color Turn>
    [[nodiscard]] int32_t searchNoTransposition(Move &bestMove, uint16_t depth, int32_t &alpha, int32_t beta);
    template<Color Turn>
    [[nodiscard]] int32_t search(uint16_t depth, int32_t alpha, int32_t beta);
};
