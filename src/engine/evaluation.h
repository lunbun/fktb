#pragma once

#include <cstdint>

#include "piece.h"
#include "board.h"

namespace Evaluation {
    // Initializes evaluation, if not already initialized.
    void maybeInit();

    // Evaluates the board for the given side, subtracting the evaluation for the other side.
    template<Color Side>
    int32_t evaluate(const Board &board);
}
