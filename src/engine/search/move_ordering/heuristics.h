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
    HistoryTable();

    // Increments the history table. Call this on beta-cutoffs of quiet moves.
    INLINE void add(Color color, const Board &board, Move move, uint16_t depth);

    // Returns the history score for the given move. The returned score is normalized based off of the total sum of all history
    // scores so that at higher depths, the history scores aren't insanely high compared to at lower depths.
    [[nodiscard]] INLINE int32_t score(Color color, PieceType type, Square to) const;

private:
    ColorMap<PieceTypeMap<SquareMap<uint32_t>>> table_;
};

// Increments the history table. Call this on beta-cutoffs of quiet moves.
INLINE void HistoryTable::add(Color color, const Board &board, Move move, uint16_t depth) {
    uint16_t score = depth * depth;
    this->table_[color][board.pieceAt(move.from()).type()][move.to()] += score;
}

// Returns the history score for the given move. The returned score is normalized based off of the total sum of all history scores
// so that at higher depths, the history scores aren't insanely high compared to at lower depths.
[[nodiscard]] INLINE int32_t HistoryTable::score(Color color, PieceType type, Square to) const {
    return static_cast<int32_t>(this->table_[color][type][to]);
}



class KillerTable {
public:
    constexpr static uint32_t MaxKillerMoves = 2;
    using Ply = std::array<Move, MaxKillerMoves>;

    KillerTable();
    ~KillerTable();

    // Resizes the killer table to the given size. This should be called at the start of each search, when the depth changes.
    void resize(uint16_t size);

    // Adds a killer move to the table. This should be called on beta-cutoffs of quiet moves.
    void add(uint16_t depth, Move move);

    [[nodiscard]] INLINE const auto &operator[](uint16_t depth) const;

private:
    // The KillerTable owns the memory for the table. We are using manual memory management to have better control over how
    // resizing works. With std::vector, when it resizes, the elements are anchored to the front of the vector and new spaces is
    // added to the end. However, because we are using depth as the index, we want the elements to be anchored to the end of the
    // vector, and new space to be added to the front. This will keep the depth indices consistent between resizes.
    uint16_t size_;
    Ply *table_;
};

const auto &KillerTable::operator[](uint16_t depth) const {
    assert(depth < this->size_);
    return this->table_[depth];
}



struct HeuristicTables {
    HistoryTable history;
    KillerTable killers;

    HeuristicTables();
};
