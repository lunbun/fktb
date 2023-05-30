#pragma once

#include <memory>
#include <random>

#include "engine/board/piece.h"
#include "engine/board/bitboard.h"
#include "engine/board/board.h"
#include "move.h"

template<Color Side>
class MoveScorer {
public:
    explicit MoveScorer(const Board &board);

    [[nodiscard]] int32_t score(Move move);

private:
    const Board &board_;

    Bitboard friendlyPawnAttacks_;
    Bitboard friendlyKnightAttacks_;

    Bitboard enemyKnights_;
    Bitboard enemyBishops_;
    Bitboard enemyRooks_;
    Bitboard enemyQueens_;

    Bitboard enemyPawnAttacks_;
    Bitboard enemyKnightAttacks_;
    Bitboard enemyBishopAttacks_;

    Bitboard enemyRookOrHigher_; // Rooks, queens, or kings
    Bitboard enemyBishopOrLowerAttacks_; // Bishop, knight, or pawn attacks
    Bitboard enemyRookOrLowerAttacks_; // Rook, bishop, knight, or pawn attacks
    Bitboard enemyQueenOrLowerAttacks_; // Queen, rook, bishop, knight, or pawn attacks
};
