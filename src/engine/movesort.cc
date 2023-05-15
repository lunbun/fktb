#include "movesort.h"

#include "piece.h"
#include "move.h"

int32_t MoveSort::scoreMove(Move move) {
    int32_t score = 0;

    if (move.isCapture()) {
        score += move.capturedPiece().material() * 10;
        score -= move.piece().material() * 2;
    }

    return score;
}
