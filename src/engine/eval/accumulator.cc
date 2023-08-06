#include "accumulator.h"

#include <cstring>
#include <cassert>

#include <immintrin.h>

#include "nnue.h"
#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"
#include "engine/board/board.h"

namespace FKTB::NNUE {

// Initializes the accumulator. It is not initialized in the constructor because otherwise it would not be possible to leave the
// accumulator uninitialized.
void Accumulator::init() {
    // Reset the accumulator to the bias values.
    std::memcpy(this->hidden1_[0], Hidden1Biases, Hidden1Size * sizeof(int16_t));
    std::memcpy(this->hidden1_[1], Hidden1Biases, Hidden1Size * sizeof(int16_t));
}



namespace {

// Computes the index of the feature for the given piece and square, then accumulates the feature's hidden1 weights into the given
// hidden1 array.
//
// Operation determines how the weights are accumulated. It should be either _mm256_add_epi16 to add the weights, or
// _mm256_sub_epi16 to subtract the weights.
template<Color Perspective, __m256i (*Operation)(__m256i, __m256i)>
INLINE void accumulateHidden1(Piece piece, Square square, int16_t *hidden1) {
    static_assert(Hidden1Size % RegisterWidth16 == 0, "Hidden1Size must be a multiple of RegisterWidth16");

    uint32_t feature = featureIndex<Perspective>(piece, square);
    const int16_t *weights = Hidden1WeightsColumnMajor + feature * Hidden1Size;

    assert(reinterpret_cast<uintptr_t>(hidden1) % ByteAlignment == 0 && "hidden1 must be aligned to ByteAlignment");
    assert(reinterpret_cast<uintptr_t>(weights) % ByteAlignment == 0 && "weights must be aligned to ByteAlignment");

    for (uint32_t i = 0; i < Hidden1Size; i += RegisterWidth16) {
        __m256i hidden1Vector = _mm256_load_si256(reinterpret_cast<const __m256i *>(hidden1 + i));
        __m256i weightsVector = _mm256_load_si256(reinterpret_cast<const __m256i *>(weights + i));
        _mm256_store_si256(reinterpret_cast<__m256i *>(hidden1 + i), Operation(hidden1Vector, weightsVector));
    }
}

} // namespace

void Accumulator::add(Piece piece, Square square) {
    accumulateHidden1<Color::White, _mm256_add_epi16>(piece, square, this->hidden1_[0]);
    accumulateHidden1<Color::Black, _mm256_add_epi16>(piece, square, this->hidden1_[1]);
}

void Accumulator::remove(Piece piece, Square square) {
    accumulateHidden1<Color::White, _mm256_sub_epi16>(piece, square, this->hidden1_[0]);
    accumulateHidden1<Color::Black, _mm256_sub_epi16>(piece, square, this->hidden1_[1]);
}

// Applies the ReLU activation function to the accumulator.
void Accumulator::relu(Color color, int16_t *output) const {
    static_assert(Hidden1Size % RegisterWidth16 == 0, "Hidden1Size must be a multiple of RegisterWidth16");

    const int16_t *hidden1 = this->hidden1_[static_cast<uint8_t>(color)];

    assert(reinterpret_cast<uintptr_t>(output) % ByteAlignment == 0 && "output must be aligned to ByteAlignment");
    assert(reinterpret_cast<uintptr_t>(hidden1) % ByteAlignment == 0 && "hidden1 must be aligned to ByteAlignment");

    __m256i zero = _mm256_setzero_si256();
    for (uint32_t i = 0; i < Hidden1Size; i += RegisterWidth16) {
        __m256i vector = _mm256_load_si256(reinterpret_cast<const __m256i *>(hidden1 + i));
        vector = _mm256_max_epi16(vector, zero);
        _mm256_store_si256(reinterpret_cast<__m256i *>(output + i), vector);
    }
}

} // namespace FKTB::NNUE
