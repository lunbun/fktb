#pragma once

#include <cstdint>
#include <array>

#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"

constexpr SquareMap<int8_t> flipVertical(const SquareMap<int8_t> &table) {
    SquareMap<int8_t> flippedTable = { 0 };
    for (uint8_t rank = 0; rank < 8; rank++) {
        for (uint8_t file = 0; file < 8; file++) {
            uint8_t index = rank * 8 + file;
            uint8_t flippedIndex = (7 - rank) * 8 + file;
            flippedTable[flippedIndex] = table[index];
        }
    }
    return flippedTable;
}

class PieceSquareTable {
public:
    constexpr explicit PieceSquareTable(SquareMap<int8_t> table) : table_(flipVertical(table), table) { }

    [[nodiscard]] INLINE constexpr const auto &operator[](Color color) const { return this->table_[color]; }

    template<Color Side>
    [[nodiscard]] INLINE constexpr int8_t get(Square square) const { return this->table_[Side][square]; }

private:
    ColorMap<SquareMap<int8_t>> table_;
};

namespace PieceSquareTables {
    extern const PieceSquareTable Pawn;
    extern const PieceSquareTable Knight;
    extern const PieceSquareTable Bishop;
    extern const PieceSquareTable Rook;
    extern const PieceSquareTable Queen;
    extern const PieceSquareTable King;
    extern const PieceTypeMap<const PieceSquareTable *> Tables;
    // TODO: Add king endgame table.

    [[nodiscard]] INLINE int8_t evaluate(Piece piece, Square square) {
        return (*Tables[piece.type()])[piece.color()][square];
    }
}
