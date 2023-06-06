#pragma once

#include <cstdint>

#include "engine/inline.h"
#include "engine/move/move.h"
#include "engine/board/color.h"
#include "engine/board/square.h"
#include "engine/board/piece.h"
#include "engine/board/bitboard.h"
#include "engine/board/board.h"

class HistoryTable {
public:
    INLINE constexpr HistoryTable();

    // Increments the history table if the move is not a capture. Call this on beta-cutoffs.
    INLINE void maybeAdd(Color color, const Board &board, Move move, uint16_t depth);

    // Returns the history score for the given move. The returned score is normalized based off of the total sum of all history
    // scores so that at higher depths, the history scores aren't insanely high compared to at lower depths.
    [[nodiscard]] INLINE int32_t score(Color color, PieceType type, Square to, uint32_t scale) const;

private:
    ColorMap<uint32_t> total_; // Sum of all history scores for each color.
    ColorMap<PieceTypeMap<SquareMap<uint32_t>>> table_;
};



// Initialize the totals to start at 1 to avoid division by zero. This won't really affect the results.
INLINE constexpr HistoryTable::HistoryTable() : total_(1, 1), table_() { }

// Increments the history table if the move is not a capture. Call this on beta-cutoffs.
INLINE void HistoryTable::maybeAdd(Color color, const Board &board, Move move, uint16_t depth) {
    if (!move.isCapture()) {
        uint16_t score = depth * depth;

        this->total_[color] += score;
        this->table_[color][board.pieceAt(move.from()).type()][move.to()] += score;
    }
}

// Returns the history score for the given move. The returned score is normalized based off of the total sum of all history scores
// so that at higher depths, the history scores aren't insanely high compared to at lower depths.
[[nodiscard]] INLINE int32_t HistoryTable::score(Color color, PieceType type, Square to, uint32_t scale) const {
    uint64_t score = this->table_[color][type][to];
    score *= scale;
    score /= this->total_[color];
    return static_cast<int32_t>(score);
}
