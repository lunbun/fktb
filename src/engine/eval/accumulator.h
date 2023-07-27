#pragma once

#include <cstdint>

#include "nnue.h"

#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"

// Forward declare Board to avoid circular dependency.
namespace FKTB {

class Board;

} // namespace FKTB

namespace FKTB::NNUE {

class Accumulator {
public:
    // Recomputes the accumulator from scratch. The accumulator can collect floating point errors over time, so this should be
    // called at the start of every search.
    void refresh(const Board &board);

    void add(Piece piece, Square square);
    void remove(Piece piece, Square square);

    [[nodiscard]] INLINE const float *operator[](Color color) const { return this->hidden1_[static_cast<uint8_t>(color)]; }

private:
    // Using raw arrays here rather than ColorMap<std::array<...>> so that we can allow the array to be uninitialized.
    float hidden1_[2][Hidden1Size];
};

} // namespace FKTB::NNUE
