#include "evaluation.h"

#include <cstdint>

#include "engine/inline.h"
#include "engine/board/square.h"
#include "engine/board/bitboard.h"

// Evaluates the board for the given side.
template<Color Side>
INLINE int32_t evaluateForSide(const Board &board) {
    int32_t score = 0;

    // Material
    score += board.material<Side>();

    // Bonus for having a bishop pair
    score += PieceMaterial::BishopPair * (board.bitboard<Side>(PieceType::Bishop).count() >= 2);

    // Piece square tables
    score += board.pieceSquareEval<Side>();

    return score;
}

// Evaluates the board for the given side, subtracting the evaluation for the other side.
template<Color Turn>
int32_t Evaluation::evaluate(const Board &board) {
    return evaluateForSide<Turn>(board) - evaluateForSide<~Turn>(board);
}



template int32_t Evaluation::evaluate<Color::White>(const Board &board);
template int32_t Evaluation::evaluate<Color::Black>(const Board &board);
