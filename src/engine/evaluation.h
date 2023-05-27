#pragma once

#include <cstdint>

#include "piece.h"
#include "board.h"

using PieceSquareTable = std::array<int32_t, 64>;

namespace Evaluation {
    extern const ColorMap<PieceSquareTable> pawnTable;
    extern const ColorMap<PieceSquareTable> knightTable;
    extern const ColorMap<PieceSquareTable> bishopTable;
    extern const ColorMap<PieceSquareTable> rookTable;
    extern const ColorMap<PieceSquareTable> queenTable;

    // Evaluates the board for the given side, subtracting the evaluation for the other side.
    template<Color Side>
    int32_t evaluate(const Board &board);
}
