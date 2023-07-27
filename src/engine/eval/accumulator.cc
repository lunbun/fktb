#include "accumulator.h"

#include <cstring>

#include "nnue.h"
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

void Accumulator::add(Piece piece, Square square) {
    // TODO: At the moment, this is very cache inefficient since the weights are not contiguous in memory (also making the weights
    //  contiguous would allow us to use SIMD).
    uint32_t whiteFeature = featureIndex<Color::White>(piece, square);
    uint32_t blackFeature = featureIndex<Color::Black>(piece, square);
    for (uint32_t i = 0; i < Hidden1Size; ++i) {
        this->hidden1_[0][i] += Hidden1Weights[i * InputSize + whiteFeature];
        this->hidden1_[1][i] += Hidden1Weights[i * InputSize + blackFeature];
    }
}

void Accumulator::remove(Piece piece, Square square) {
    // TODO: At the moment, this is very cache inefficient since the weights are not contiguous in memory (also making the weights
    //  contiguous would allow us to use SIMD).
    uint32_t whiteFeature = featureIndex<Color::White>(piece, square);
    uint32_t blackFeature = featureIndex<Color::Black>(piece, square);
    for (uint32_t i = 0; i < Hidden1Size; ++i) {
        this->hidden1_[0][i] -= Hidden1Weights[i * InputSize + whiteFeature];
        this->hidden1_[1][i] -= Hidden1Weights[i * InputSize + blackFeature];
    }
}

} // namespace FKTB::NNUE
