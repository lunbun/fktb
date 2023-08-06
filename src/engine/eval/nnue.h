#pragma once

#include <cstdint>

#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"

namespace FKTB::NNUE {

class Accumulator;

constexpr uint32_t InputSize = 768;
constexpr uint32_t Hidden1Size = 256;
constexpr uint32_t Hidden2Size = 32;
constexpr uint32_t Hidden3Size = 8;
constexpr uint32_t OutputSize = 1;

// We're just going to assume that the 16-bit values are never going to overflow. They very well could if the unscaled equivalent
// of the value exceeded INT16_MAX / InputsScale (at the moment is about 256), but that seems pretty unlikely.
//
// IMPORTANT NOTE: Do not increase InputsScaleLog2 above 6. It causes strange behavior in the network, which I believe is caused
// by the accumulator overflowing/underflowing (although I haven't confirmed this).
//
// Also, it may be dangerous to increase WeightsScaleLog2 too high, as it may cause the 32-bit accumulators to overflow/underflow
// during linear layer propagation.
constexpr uint32_t InputsScaleLog2 = 6;
constexpr uint32_t InputsScale = 1 << InputsScaleLog2;
constexpr uint32_t WeightsScaleLog2 = 7;
constexpr uint32_t WeightsScale = 1 << WeightsScaleLog2;

constexpr uint32_t RegisterWidth8 = 32;         // Number of 8-bit values that fit into a 256-bit AVX register.
constexpr uint32_t RegisterWidth16 = 16;        // Number of 16-bit values that fit into a 256-bit AVX register.
constexpr uint32_t RegisterWidth8_128 = 16;     // Number of 8-bit values that fit into a 128-bit SSE register.
constexpr uint32_t RegisterWidth16_128 = 8;     // Number of 16-bit values that fit into a 128-bit SSE register.
constexpr uint32_t ByteAlignment = 32;          // Alignment of AVX registers in bytes.

// Weights are by default stored in row-major order, where the weights are indexed by weights[output][input].
extern int16_t Hidden1Weights[Hidden1Size * InputSize];
extern int16_t Hidden1Biases[Hidden1Size];
extern int16_t Hidden2Weights[Hidden2Size * Hidden1Size];
extern int32_t Hidden2Biases[Hidden2Size];
extern int16_t Hidden3Weights[Hidden3Size * Hidden2Size];
extern int32_t Hidden3Biases[Hidden3Size];
extern int16_t OutputWeights[OutputSize * Hidden3Size];
extern int32_t OutputBiases[OutputSize];

// Weights stored in column-major order, where the weights are indexed by weights[input][output].
extern int16_t Hidden1WeightsColumnMajor[InputSize * Hidden1Size];

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
