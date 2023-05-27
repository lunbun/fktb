#pragma once

#include "piece.h"
#include "bitboard.h"
#include "board.h"
#include "move.h"

template<Color Side>
class MoveScorer {
public:
    explicit MoveScorer(const Board &board);

    int32_t score(Move move);

private:
    const Board &board_;

    Bitboard friendlyPawnAttacks_;
    Bitboard enemyPawnAttacks_;
};
