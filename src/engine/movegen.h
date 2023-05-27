#pragma once

#include <array>

#include "move.h"
#include "bitboard.h"
#include "board.h"

template<Color Side>
class MoveGenerator {
public:
    MoveGenerator(const Board &board, MovePriorityQueueStack &moves);

    void generate();

private:
    const Board &board_;
    MovePriorityQueueStack &moves_;

    Bitboard friendly_;
    Bitboard enemy_;
    Bitboard occupied_;
    Bitboard empty_;

    void serializeBitboard(Square from, Bitboard bitboard);
    void generatePawnMoves(Square square);
    void generateAllPawnMoves();

    template<Bitboard (*GenerateAttacks)(Square)>
    void generateOffsetMoves(Piece piece);

    template<Bitboard (*GenerateAttacks)(Square, Bitboard)>
    void generateSlidingMoves(Piece piece);
};
