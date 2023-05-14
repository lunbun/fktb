#include "movesort.h"

#include <vector>
#include <algorithm>
#include <optional>

#include "piece.h"
#include "move.h"
#include "transposition.h"

struct MoveScore {
    Move move;
    int32_t score;
};

inline int32_t scoreMove(Move move) {
    int32_t score = 0;

    if (move.isCapture()) {
        score += move.capturedPiece().material() * 10;
        score -= move.piece().material() * 2;
    }

    return score;
}

inline int32_t scoreMove(Move move, TranspositionTable::Entry *entry) {
    int32_t score = scoreMove(move);

    if (entry != nullptr && move == entry->bestMove) {
        score += 1000000;
    }

    return score;
}

inline void sortMovesAndCopyIntoMoveList(std::vector<MoveScore> &scores, MoveListStack &moves) {
    // Sort the moves by score.
    std::sort(scores.begin(), scores.end(), [](const MoveScore &a, const MoveScore &b) {
        return a.score > b.score;
    });

    // Copy the moves back into the move list.
    for (uint32_t i = 0; i < moves.size(); ++i) {
        moves.unsafeSet(i, scores[i].move);
    }
}

// Heuristically sorts moves to improve alpha-beta pruning.
void MoveSort::sort(MoveListStack &moves) {
    // Copy the moves into a vector, and score them.
    std::vector<MoveScore> scores;
    scores.reserve(moves.size());

    for (uint32_t i = 0; i < moves.size(); ++i) {
        Move move = moves.unsafeAt(i);
        scores.push_back({ move, scoreMove(move ) });
    }

    sortMovesAndCopyIntoMoveList(scores, moves);
}

// Heuristically sorts moves to improve alpha-beta pruning.
// Also uses the previous iteration's transposition table to better score moves.
void MoveSort::sort(Board &board, MoveListStack &moves, TranspositionTable &previousIterationTable) {
    // Copy the moves into a vector, and score them.
    std::vector<MoveScore> scores;
    scores.reserve(moves.size());

    // No need to lock the table, since we're only reading from it.
    TranspositionTable::Entry *entry = previousIterationTable.load(board.hash());
    for (uint32_t i = 0; i < moves.size(); ++i) {
        Move move = moves.unsafeAt(i);
        scores.push_back({ move, scoreMove(move, entry) });
    }

    sortMovesAndCopyIntoMoveList(scores, moves);
}
