#pragma once

#include <cstdint>

#include "engine/board/color.h"
#include "engine/board/board.h"

namespace FKTB::NNUE {

// Loads the neural network.
void init();

// Evaluates the given board position from the perspective of the given side.
template<Color Side>
int32_t evaluate(const Board &board);

} // namespace FKTB::NNUE
