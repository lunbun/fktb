#pragma once

#include <cstdint>

#include "engine/inline.h"
#include "engine/board/square.h"
#include "engine/board/piece.h"
#include "engine/board/board.h"

namespace FKTB::Evaluation {

// Evaluates the board for the given side, subtracting the evaluation for the other side.
template<Color Side>
int32_t evaluate(const Board &board, int32_t alpha, int32_t beta);

} // namespace FKTB::Evaluation
