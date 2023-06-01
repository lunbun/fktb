#pragma once

#include <array>

#include "move.h"
#include "move_list.h"
#include "engine/board/board.h"
#include "engine/board/bitboard.h"

namespace MoveGeneration {
    // Generates all pseudo-legal moves for the given side, excluding quiet moves if ExcludeQuiet is true.
    // Returns the pointer to the end of the move list.
    template<Color Side, bool ExcludeQuiet>
    [[nodiscard]] MoveEntry *generatePseudoLegal(const Board &board, MoveEntry *moves);

    // Generates all legal moves for the given side, excluding quiet moves if ExcludeQuiet is true.
    // Returns the pointer to the end of the move list.
    template<Color Side, bool ExcludeQuiet>
    [[nodiscard]] MoveEntry *generateLegal(Board &board, MoveEntry *moves);

    // Generates all moves for the turn.
    [[nodiscard]] RootMoveList generateLegalRoot(Board &board);
}
