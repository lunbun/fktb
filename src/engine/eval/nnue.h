#pragma once

#include <cstdint>

#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"

namespace FKTB::NNUE {

class Accumulator;

constexpr uint32_t InputSize = 768;
constexpr uint32_t Hidden1Size = 512;
constexpr uint32_t Hidden2Size = 64;
constexpr uint32_t Hidden3Size = 32;
constexpr uint32_t OutputSize = 1;

extern const float *Hidden1Weights;
extern const float *Hidden1Biases;
extern const float *Hidden2Weights;
extern const float *Hidden2Biases;
extern const float *Hidden3Weights;
extern const float *Hidden3Biases;
extern const float *OutputWeights;
extern const float *OutputBiases;

// Loads the neural network.
void init();



// Returns the feature index for the given piece and square from the perspective of the given side.
template<Color Perspective>
INLINE constexpr uint32_t featureIndex(Piece piece, Square square) {
    Color color = piece.color();

    // Neural network does not have a concept of side-to-move, it expects the board to be from the side-to-move's perspective. So
    // we need to flip the board vertically if Perspective is black.
    if constexpr (Perspective == Color::Black) {
        square = { square.file(), static_cast<uint8_t>(7 - square.rank()) };

        // Also flip the color of the piece since we flipped the board.
        color = ~color;
    }

    return static_cast<uint32_t>(color * 6 + piece.type()) * 64 + static_cast<uint32_t>(square);
}

// Evaluates the given board position from the perspective of the given side.
int32_t evaluate(Color side, const NNUE::Accumulator &accumulator);

} // namespace FKTB::NNUE
