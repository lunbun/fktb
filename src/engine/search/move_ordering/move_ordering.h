#pragma once

#include <cstdint>
#include <memory>

#include "history.h"
#include "engine/inline.h"
#include "engine/move/move.h"
#include "engine/move/move_list.h"

namespace MoveOrdering {
    // @formatter:off
    namespace Flags {
        constexpr uint32_t History  = 0b0001;   // Include history heuristic?
    }
    // @formatter:on

    // Only these sets of flags will link properly with the score method.
    namespace Type {
        // Scores without history heuristic.
        constexpr uint32_t NoHistory = 0;

        // Scores with history heuristic.
        constexpr uint32_t History = Flags::History;
    }

    // Scores the moves in the given list.
    // Pass nullptr into history if history heuristic is not used.
    template<Color Side, uint32_t Flags>
    void score(MovePriorityQueue &moves, const Board &board, const HistoryTable *history);

    // Scores the moves in the given list.
    // Pass nullptr into history if history heuristic is not used.
    template<uint32_t Flags>
    void score(RootMoveList &moves, const Board &board, const HistoryTable *history);
}
