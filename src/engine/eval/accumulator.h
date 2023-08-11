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

class alignas(ByteAlignment) Accumulator {
public:
    // The default constructor leaves the accumulator uninitialized. To initialize it, call init().
    INLINE Accumulator() { }

    // Initializes the accumulator. It is not initialized in the constructor because otherwise it would not be possible to leave
    // the accumulator uninitialized.
    void init();

    void add(Piece piece, Square square);
    void remove(Piece piece, Square square);

    // Applies the ReLU activation function to the accumulator.
    void relu(Color color, int16_t *output) const;

private:
    // Using raw arrays here rather than ColorMap<std::array<...>> so that we can allow the array to be uninitialized.
    int16_t hidden1_[2][Hidden1Size];
};

} // namespace FKTB::NNUE
