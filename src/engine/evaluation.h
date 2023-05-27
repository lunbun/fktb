#pragma once

#include <cstdint>

#include "piece.h"
#include "board.h"

namespace Evaluation {
    // Evaluates the board for the given side, subtracting the evaluation for the other side.
    template<Color Side>
    int32_t evaluate(const Board &board);
}
