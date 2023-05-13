#pragma once

#include "board.h"
#include "move.h"
#include "sync_transposition.h"

namespace MoveSort {
    // Heuristically sorts moves to improve alpha-beta pruning.
    void sort(MoveListStack &moves);

    // Heuristically sorts moves to improve alpha-beta pruning.
    // Also uses the previous iteration's transposition table to better score moves.
    void sort(Board &board, MoveListStack &moves, SynchronizedTranspositionTable &previousIterationTable);
}
