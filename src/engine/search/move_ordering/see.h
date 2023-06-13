#pragma once

#include <cstdint>

#include "engine/inline.h"
#include "engine/move/move.h"
#include "engine/board/color.h"
#include "engine/board/board.h"

namespace See {
    // Performs a static exchange evaluation on the given square. The square must be occupied by a piece of the defender's color.
    template<Color Defender>
    int32_t evaluate(Square square, const Board &board);

    // Performs a static exchange evaluation on the given move. The move does not have to be a capture.
    template<Color Side>
    int32_t evaluate(Move move, Board &board);
}
