#include "evaluation.h"

#include <cstdint>
#include <array>

#include "bitboard.h"
#include "inline.h"

using BonusTable = std::array<int32_t, 64>;

BonusTable knightBonuses;
BonusTable bishopBonuses;
BonusTable queenBonuses;

// Generates attacks for a sliding piece on an empty board.
// Used for initBonuses templates.
template<Bitboard (*GenerateAttacks)(Square, Bitboard)>
Bitboard bindEmpty(Square square) {
    return GenerateAttacks(square, 0);
}

// Initializes the bonus table based on the number of attacks for each square.
template<Bitboard (*GenerateAttacks)(Square)>
void initAttackBonuses(BonusTable &table) {
    table.fill(0);

    // Add the number of attacks for each square.
    int32_t minAttacks = INT32_MAX;
    int32_t maxAttacks = INT32_MIN;
    for (uint8_t square = 0; square < 64; square++) {
        int32_t attacks = GenerateAttacks(square).count();

        minAttacks = std::min(minAttacks, attacks);
        maxAttacks = std::max(maxAttacks, attacks);
        table[square] = attacks;
    }

    // Subtract the average number of attacks for each square to make the bonuses centered around 0.
    int32_t averageAttacks = (minAttacks + maxAttacks) / 2;
    for (uint8_t square = 0; square < 64; square++) {
        table[square] -= averageAttacks;
    }

    // Scale the bonuses (to make them more significant, since evaluation is in centipawns).
    for (uint8_t square = 0; square < 64; square++) {
        table[square] *= 3;
    }
}

void Evaluation::maybeInit() {
    static bool isInitialized = false;

    if (isInitialized) {
        return;
    }

    isInitialized = true;

    Bitboards::maybeInit();

    initAttackBonuses<Bitboards::knight>(knightBonuses);
    initAttackBonuses<bindEmpty<Bitboards::bishop>>(bishopBonuses);
    initAttackBonuses<bindEmpty<Bitboards::queen>>(queenBonuses);
}



template<Color Side, PieceType Type, const BonusTable &Table>
INLINE int32_t evaluatePieceBonuses(const Board &board) {
    int32_t score = 0;

    Bitboard pieces = board.bitboard<Side>(Type);
    while (pieces) {
        Square square = pieces.bsfReset();
        score += Table[square];
    }

    return score;
}

template<Color Side>
INLINE int32_t evaluateRookBonuses(const Board &board) {
    // Bonus for having a rook on the 7th rank, since there are usually lots of free pawns to attack.
    Bitboard rooks = board.bitboard<Side>(PieceType::Rook);
    if constexpr (Side == Color::White) {
        rooks = rooks & Bitboards::Rank7;
    } else {
        rooks = rooks & Bitboards::Rank2;
    }

    return rooks.count() * PieceMaterial::RookOn7th;
}

// Evaluates the board for the given side.
template<Color Side>
INLINE int32_t evaluateForSide(const Board &board) {
    int32_t score = 0;

    // Material
    score += board.material<Side>();

    // Bonus for having a bishop pair
    score += PieceMaterial::BishopPair * (board.bitboard<Side>(PieceType::Bishop).count() >= 2);

    score += evaluatePieceBonuses<Side, PieceType::Knight, knightBonuses>(board);
    score += evaluatePieceBonuses<Side, PieceType::Bishop, bishopBonuses>(board);
    score += evaluateRookBonuses<Side>(board);
    score += evaluatePieceBonuses<Side, PieceType::Queen, queenBonuses>(board);

    return score;
}

// Evaluates the board for the given side, subtracting the evaluation for the other side.
template<Color Turn>
int32_t Evaluation::evaluate(const Board &board) {
    return evaluateForSide<Turn>(board) - evaluateForSide<~Turn>(board);
}



template int32_t Evaluation::evaluate<Color::White>(const Board &board);
template int32_t Evaluation::evaluate<Color::Black>(const Board &board);
