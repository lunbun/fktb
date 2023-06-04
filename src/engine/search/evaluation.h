#pragma once

#include <cstdint>

#include "engine/inline.h"
#include "engine/board/square.h"
#include "engine/board/piece.h"
#include "engine/board/board.h"

using PieceSquareTable = SquareMap<int32_t>;

namespace Evaluation {
    extern const ColorMap<PieceSquareTable> pawnTable;
    extern const ColorMap<PieceSquareTable> knightTable;
    extern const ColorMap<PieceSquareTable> bishopTable;
    extern const ColorMap<PieceSquareTable> rookTable;
    extern const ColorMap<PieceSquareTable> queenTable;
    extern const ColorMap<PieceSquareTable> kingTable;
    // TODO: Add king endgame table.

    // Evaluates the board for the given side, subtracting the evaluation for the other side.
    template<Color Side>
    int32_t evaluate(const Board &board);
}
