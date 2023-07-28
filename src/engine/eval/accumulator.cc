#include "accumulator.h"

#include <cstring>

#include <immintrin.h>

#include "nnue.h"
#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"
#include "engine/board/board.h"

namespace FKTB::NNUE {

// Recomputes the accumulator from scratch. The accumulator can collect floating point errors over time, so this should be
// called at the start of every search.
void Accumulator::refresh(const Board &board) {
    // Reset the accumulator to the bias values.
    std::memcpy(this->hidden1_[0], Hidden1Biases, Hidden1Size * sizeof(float));
    std::memcpy(this->hidden1_[1], Hidden1Biases, Hidden1Size * sizeof(float));

    // Add the pieces to the accumulator.
    for (Color color = Color::White; color <= Color::Black; color = static_cast<Color>(color + 1)) {
        for (PieceType type = PieceType::Pawn; type <= PieceType::Queen; type = static_cast<PieceType>(type + 1)) {
            Piece piece(color, type);

            Bitboard pieces = board.bitboard(piece);
            for (Square square : pieces) {
                this->add(piece, square);
            }
        }

        // Add the king to the accumulator (if the king has a valid square).
        Square king = board.king(color);
        if (king.isValid()) {
            this->add(Piece::king(color), king);
        }
    }
}



namespace {

// Computes the index of the feature for the given piece and square, then accumulates the feature's hidden1 weights into the given
// float array.
//
// Operation determines how the weights are accumulated. It should be either _mm256_add_ps to add the weights, or _mm256_sub_ps to
// subtract the weights.
template<Color Perspective, __m256 (*Operation)(__m256, __m256)>
INLINE void accumulateHidden1(Piece piece, Square square, float *hidden1) {
    static_assert(Hidden1Size % 8 == 0, "Hidden1Size must be a multiple of 8");

    uint32_t feature = featureIndex<Perspective>(piece, square);
    const float *weights = Hidden1WeightsColumnMajor + feature * Hidden1Size;
    for (uint32_t i = 0; i < Hidden1Size; i += 8) {
        __m256 hidden1Vector = _mm256_load_ps(hidden1 + i);
        __m256 weightsVector = _mm256_load_ps(weights + i);
        _mm256_store_ps(hidden1 + i, Operation(hidden1Vector, weightsVector));
    }
}

} // namespace

void Accumulator::add(Piece piece, Square square) {
    accumulateHidden1<Color::White, _mm256_add_ps>(piece, square, this->hidden1_[0]);
    accumulateHidden1<Color::Black, _mm256_add_ps>(piece, square, this->hidden1_[1]);
}

void Accumulator::remove(Piece piece, Square square) {
    accumulateHidden1<Color::White, _mm256_sub_ps>(piece, square, this->hidden1_[0]);
    accumulateHidden1<Color::Black, _mm256_sub_ps>(piece, square, this->hidden1_[1]);
}

} // namespace FKTB::NNUE
