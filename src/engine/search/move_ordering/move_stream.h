#pragma once

#include <cstdint>

#include "heuristics.h"
#include "engine/move/move_list.h"

// Combines all move ordering stages into a single stream of moves, lazily generating moves as needed.
class MoveStream {
public:
    MoveStream(Board &board, Move hashMove, const HeuristicTables &heuristics, uint16_t depth);

    // Returns the next move in the stream, or Move::invalid() if there are no more moves.
    template<Color Side>
    [[nodiscard]] Move next();

    // TODO: This is just temporary while I get MoveStream working
    [[nodiscard]] INLINE uint8_t stage() const { return static_cast<uint8_t>(this->stage_); }

private:
    // @formatter:off
    enum class Stage : uint8_t {
        HashMove                = 0,
        TacticalGeneration      = 1,
        Tactical                = 2,
        Killers                 = 3,
        QuietGeneration         = 4,
        Quiet                   = 5,
        End                     = 6
    };
    // @formatter:on

    Stage stage_;
    Board &board_;

    Move hashMove_;
    AlignedMoveEntry moveBuffer_[MaxMoveCount];
    MovePriorityQueue queue_;
    const HeuristicTables &heuristics_;
    uint16_t depth_;
    uint8_t killerIndex_;

    // Bug with clang-tidy apparently, it wants me to insert "const" before the MoveStream::Stage return type, but that would be
    // invalid C++.
    // NOLINTNEXTLINE
    friend Stage operator++(Stage &stage, int);
};
