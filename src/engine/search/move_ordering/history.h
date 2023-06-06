#pragma once

#include <cstdint>

#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/square.h"
#include "engine/board/piece.h"
#include "engine/board/bitboard.h"
#include "engine/board/board.h"

class HistoryTable {
public:
    INLINE constexpr HistoryTable();

    // Adds to the history table. Call on beta-cutoffs.
    template<Color Side>
    INLINE void add(PieceType type, Square to, uint16_t depth);

    // Returns the history score for the given move. The history score is normalized based off of total_, so that at higher
    // depths the history score isn't insanely high compared to at lower depths.
    template<Color Side>
    [[nodiscard]] INLINE int32_t moveScoreAt(PieceType type, Square to) const;

private:
    ColorMap<uint32_t> total_; // Sum of all entries in the table.
    ColorMap<PieceTypeMap<SquareMap<uint32_t>>> table_;
};

// Total starts at 1 to avoid division by 0. This won't really affect the results.
INLINE constexpr HistoryTable::HistoryTable() : total_(1, 1), table_() { }

template<Color Side>
INLINE void HistoryTable::add(PieceType type, Square to, uint16_t depth) {
    uint16_t score = depth * depth;

    this->total_[Side] += score;
    this->table_[Side][type][to] += score;
}

template<Color Side>
[[nodiscard]] INLINE int32_t HistoryTable::moveScoreAt(PieceType type, Square to) const {
    uint64_t score = this->table_[Side][type][to];
    score *= 5000ULL;   // TODO: Further tune this to maximize pruning.
    score /= this->total_[Side];
    return static_cast<int32_t>(score);
}
