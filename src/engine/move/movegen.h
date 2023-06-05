#pragma once

#include <array>
#include <cstdint>

#include "move.h"
#include "move_list.h"
#include "engine/board/board.h"
#include "engine/board/bitboard.h"

namespace MoveGeneration {
    // @formatter:off
    namespace Flags {
        constexpr uint32_t Quiet    = 0b0001;   // Only quiet moves.
        constexpr uint32_t Tactical = 0b0010;   // Only tactical moves (non-quiet).
        constexpr uint32_t Legal    = 0b0100;   // Only legal moves.
        constexpr uint32_t Evasion  = 0b1000;   // Only check evasions.
    }
    // @formatter:on

    // Only these sets of flags will link properly with the generate method.
    namespace Type {
        // Generates all pseudo-legal moves.
        // Note: Pseudo-legal moves generation is not optimized well. Only use for debugging.
        constexpr uint32_t PseudoLegal = Flags::Tactical | Flags::Quiet;

        // Generates all legal quiet moves.
        constexpr uint32_t Quiet = Flags::Legal | Flags::Quiet;

        // Generates all legal tactical (non-quiet) moves.
        constexpr uint32_t Tactical = Flags::Legal | Flags::Tactical;

        // Generates all legal moves.
        constexpr uint32_t Legal = Flags::Legal | Flags::Tactical | Flags::Quiet;
    }

    // Generates moves for the given side and flags.
    // Returns the pointer to the end of the move list.
    template<Color Side, uint32_t Flags>
    [[nodiscard]] MoveEntry *generate(Board &board, MoveEntry *moves);

    // Generates all legal moves for the turn.
    [[nodiscard]] RootMoveList generateLegalRoot(Board &board);
}
