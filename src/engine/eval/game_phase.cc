#include "game_phase.h"

#include <cstdint>
#include <algorithm>

#include "engine/board/board.h"

// https://www.chessprogramming.org/Tapered_Eval#Implementation_example
uint16_t TaperedEval::calculateContinuousPhase(const Board &board) {
    // Pawns are not included in the game phase calculation, since there are usually lots of pawns in the end game.
    constexpr uint16_t KnightWeight = 1;
    constexpr uint16_t BishopWeight = 1;
    constexpr uint16_t RookWeight = 2;
    constexpr uint16_t QueenWeight = 4;
    constexpr uint16_t MaxWeight = (4 * KnightWeight) + (4 * BishopWeight) + (4 * RookWeight) + (2 * QueenWeight);

    // The game should still be considered the end game even if there are still a few pieces left.
    constexpr uint16_t EndGameWeight = MaxWeight - ((2 * BishopWeight) + (1 * RookWeight));

    // Weight will be
    //  - 0 if there are no pieces on the board
    //  - MaxWeight if all pieces are on the board
    uint16_t weight = MaxWeight;

    weight -= KnightWeight * board.composite(PieceType::Knight).count();
    weight -= BishopWeight * board.composite(PieceType::Bishop).count();
    weight -= RookWeight * board.composite(PieceType::Rook).count();
    weight -= QueenWeight * board.composite(PieceType::Queen).count();

    return std::clamp((weight * 256) / EndGameWeight, 0, 256);
}
