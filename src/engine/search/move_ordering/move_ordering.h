#pragma once

#include <cstdint>
#include <memory>

#include "heuristics.h"
#include "engine/inline.h"
#include "engine/move/move.h"
#include "engine/move/move_list.h"

namespace FKTB::MoveOrdering {

// @formatter:off

namespace Flags {
    constexpr uint32_t Quiet    = 0b0001;   // Use quiet move ordering.
    constexpr uint32_t Tactical = 0b0010;   // Use tactical move ordering.
    constexpr uint32_t History  = 0b0100;   // Use history heuristic.
} // namespace Flags

// Only these sets of flags will link properly with the score method.
namespace Type {
    // Scores quiet moves with history heuristic.
    constexpr uint32_t Quiet = Flags::Quiet | Flags::History;

    // Scores tactical moves without history heuristic.
    constexpr uint32_t Tactical = Flags::Tactical;

    // Scores all moves with history heuristic.
    constexpr uint32_t All = Flags::Quiet | Flags::Tactical | Flags::History;

    // Scores all moves without history heuristic.
    constexpr uint32_t AllNoHistory = Flags::Quiet | Flags::Tactical;
} // namespace Type

// @formatter:on

// Scores the moves in the given list.
// Pass nullptr into history if history heuristic is not used.
template<Color Side, uint32_t Flags>
void score(MovePriorityQueue &moves, Board &board, const HistoryTable *history);

// Scores the moves in the given list.
// Pass nullptr into history if history heuristic is not used.
template<uint32_t Flags>
void score(RootMoveList &moves, Board &board, const HistoryTable *history);

} // namespace FKTB::MoveOrdering
