#pragma once

#include "move.h"
#include "board.h"

namespace MoveGenerator {
    void generate(const Board &board, MovePriorityQueueStack &moves);
}
