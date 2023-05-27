#include "evaluation.h"

#include <cstdint>
#include <array>

#include "bitboard.h"
#include "inline.h"

using PieceSquareTable = std::array<int32_t, 64>;

// Tables taken from https://www.chessprogramming.org/Simplified_Evaluation_Function
//
// Important Note: The tables are actually flipped vertically from what is visually shown here since index 0 (rank 1)
// is at the top, and index 63 (rank 8) is at the bottom.
PieceSquareTable pawnTable = {
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0
};
PieceSquareTable knightTable = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};
PieceSquareTable bishopTable = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};
PieceSquareTable rookTable = {
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0
};
PieceSquareTable queenTable = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
    0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};



template<Color Side, PieceType Type, const PieceSquareTable &Table>
INLINE int32_t evaluatePieceBonuses(const Board &board) {
    int32_t score = 0;

    Bitboard pieces = board.bitboard<Side>(Type);
    if constexpr (Side == Color::White) {
        // TODO: Flip the table instead of the bitboard.
        pieces = pieces.flipVertical();
    }

    while (pieces) {
        Square square = pieces.bsfReset();
        score += Table[square];
    }

    return score;
}

// Evaluates the board for the given side.
template<Color Side>
INLINE int32_t evaluateForSide(const Board &board) {
    int32_t score = 0;

    // Material
    score += board.material<Side>();

    // Bonus for having a bishop pair
    score += PieceMaterial::BishopPair * (board.bitboard<Side>(PieceType::Bishop).count() >= 2);

    score += evaluatePieceBonuses<Side, PieceType::Pawn, pawnTable>(board);
    score += evaluatePieceBonuses<Side, PieceType::Knight, knightTable>(board);
    score += evaluatePieceBonuses<Side, PieceType::Bishop, bishopTable>(board);
    score += evaluatePieceBonuses<Side, PieceType::Rook, rookTable>(board);
    score += evaluatePieceBonuses<Side, PieceType::Queen, queenTable>(board);

    return score;
}

// Evaluates the board for the given side, subtracting the evaluation for the other side.
template<Color Turn>
int32_t Evaluation::evaluate(const Board &board) {
    return evaluateForSide<Turn>(board) - evaluateForSide<~Turn>(board);
}



template int32_t Evaluation::evaluate<Color::White>(const Board &board);
template int32_t Evaluation::evaluate<Color::Black>(const Board &board);
