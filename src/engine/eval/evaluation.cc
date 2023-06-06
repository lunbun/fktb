#include "evaluation.h"

#include <cstdint>

#include "engine/inline.h"
#include "engine/board/square.h"
#include "engine/board/bitboard.h"

template<Color Side>
INLINE int32_t evaluatePawnShield(const Board &board);

// TODO: More complex king safety evaluation.
template<Color Side, uint64_t Mask>
INLINE int32_t evaluatePawnShieldWithMask(const Board &board) {
    constexpr Bitboard Mask2 = Bitboard(Mask).shiftForward<Side>(1);

    int32_t score = 0;

    Bitboard pawns = board.bitboard<Side>(PieceType::Pawn);
    Bitboard pawnShield1 = pawns & Mask;
    Bitboard pawnShield2 = pawns & Mask2;

    // Remove doubled pawns from the second pawn shield.
    pawnShield2 &= ~pawnShield1.shiftForward<Side>(1);

    uint8_t pawnShield1Count = pawnShield1.count();
    uint8_t pawnShield2Count = pawnShield2.count();
    uint8_t missingShields = 3 - (pawnShield1Count + pawnShield2Count);

    score += 8 * pawnShield1Count;
    score += 4 * pawnShield2Count;
    score -= 10 * missingShields;

    return score;
}

template<>
INLINE int32_t evaluatePawnShield<Color::White>(const Board &board) {
    Square king = board.king<Color::White>();

    // Only evaluate pawn shield if king is on first two ranks.
    if (king.rank() > 1) {
        return 0;
    }

    if (king.file() >= 5) { // King is on the king-side.
        return evaluatePawnShieldWithMask<Color::White, Bitboards::F2 | Bitboards::G2 | Bitboards::H2>(board);
    } else if (king.file() <= 2) { // King is on the queen-side.
        return evaluatePawnShieldWithMask<Color::White, Bitboards::A2 | Bitboards::B2 | Bitboards::C2>(board);
    } else {
        return 0;
    }
}

template<>
INLINE int32_t evaluatePawnShield<Color::Black>(const Board &board) {
    Square king = board.king<Color::Black>();

    // Only evaluate pawn shield if king is on first two ranks.
    if (king.rank() < 6) {
        return 0;
    }

    if (king.file() >= 5) { // King is on the king-side.
        return evaluatePawnShieldWithMask<Color::Black, Bitboards::F7 | Bitboards::G7 | Bitboards::H7>(board);
    } else if (king.file() <= 2) { // King is on the queen-side.
        return evaluatePawnShieldWithMask<Color::Black, Bitboards::A7 | Bitboards::B7 | Bitboards::C7>(board);
    } else {
        return 0;
    }
}

template<Color Side>
INLINE int32_t evaluateKingSafety(const Board &board) {
    int32_t score = 0;

    // Pawn shield
    score += evaluatePawnShield<Side>(board);

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

    // Piece square tables
    score += board.pieceSquareEval<Side>();

    // King safety
    score += evaluateKingSafety<Side>(board);

    return score;
}

// Evaluates the board for the given side, subtracting the evaluation for the other side.
template<Color Turn>
int32_t Evaluation::evaluate(const Board &board) {
    return evaluateForSide<Turn>(board) - evaluateForSide<~Turn>(board);
}



template int32_t Evaluation::evaluate<Color::White>(const Board &board);
template int32_t Evaluation::evaluate<Color::Black>(const Board &board);
