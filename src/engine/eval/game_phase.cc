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

    // The total weight of all the starting pieces.
    constexpr uint16_t MaxWeight = (4 * KnightWeight) + (4 * BishopWeight) + (4 * RookWeight) + (2 * QueenWeight);

    // The total weight of all the pieces lost before we stop considering the game to be in the middle game.
    constexpr uint16_t MiddleGameWeight = (4 * KnightWeight);

    // The total weight of the all the pieces lost before we start considering the game to be in the end game.
    constexpr uint16_t EndGameWeight = (4 * KnightWeight) + (3 * BishopWeight) + (2 * RookWeight) + (2 * QueenWeight);

    // lostPiecesWeight is the total weight of all the lost pieces. Will be between:
    //  - 0 if no pieces are lost (all starting pieces are still on the board)
    //  - MaxWeight if all pieces are lost (no pieces are on the board)
    uint16_t lostPiecesWeight = MaxWeight;

    lostPiecesWeight -= KnightWeight * board.composite(PieceType::Knight).count();
    lostPiecesWeight -= BishopWeight * board.composite(PieceType::Bishop).count();
    lostPiecesWeight -= RookWeight * board.composite(PieceType::Rook).count();
    lostPiecesWeight -= QueenWeight * board.composite(PieceType::Queen).count();

    // lostPiecesWeight is mapped from the range [MiddleGameWeight, EndGameWeight] to [0, 256].
    lostPiecesWeight = std::clamp(lostPiecesWeight, MiddleGameWeight, EndGameWeight);

    return ((lostPiecesWeight - MiddleGameWeight) * 256) / (EndGameWeight - MiddleGameWeight);
}
