#pragma once

#include <array>

#include "move.h"
#include "move_queue.h"
#include "bitboard.h"
#include "board.h"

template<Color Side, bool ExcludeQuiet>
class MoveGenerator {
public:
    MoveGenerator() = delete;
    MoveGenerator(const Board &board, MovePriorityQueueStack &moves);

    void generate();

private:
    const Board &board_;
    MovePriorityQueueStack &moves_;

    Bitboard friendly_;
    Bitboard enemy_;
    Bitboard occupied_;
    Bitboard empty_;

    void serializeQuiet(Square from, Bitboard quiet);
    void serializeCaptures(Square from, Bitboard captures);
    void serializePromotions(Square from, Bitboard promotions);
    // Serializes all moves in a bitboard, both quiet and captures (will not serialize quiet if in ExcludeQuiet mode)
    void serializeBitboard(Square from, Bitboard bitboard);

    void generatePawnMoves(Square square);
    void generateAllPawnMoves();

    template<Bitboard (*GenerateAttacks)(Square)>
    void generateOffsetMoves(Piece piece);

    template<Bitboard (*GenerateAttacks)(Square, Bitboard)>
    void generateSlidingMoves(Piece piece);
};
