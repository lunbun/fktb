#include "move_ordering.h"

#include <cassert>

#include "see.h"
#include "engine/move/move.h"
#include "engine/move/move_list.h"
#include "engine/board/piece.h"
#include "engine/board/board.h"
#include "engine/eval/game_phase.h"
#include "engine/eval/piece_square_table.h"

// Class is used for convenience so that we don't have to pass around the board, history table, and bitboards separately as
// parameters.
template<Color Side, uint32_t Flags>
class MoveScorer {
public:
    explicit MoveScorer(Board &board, const HistoryTable *history);

    [[nodiscard]] int32_t score(Move move);

private:
    Board &board_;
    const HistoryTable *history_;

    uint16_t gamePhase_;

    [[nodiscard]] INLINE const HistoryTable &history() const {
        static_assert(Flags & MoveOrdering::Flags::History, "History flag is not set.");
        return *this->history_;
    }
};

template<Color Side, uint32_t Flags>
MoveScorer<Side, Flags>::MoveScorer(Board &board, const HistoryTable *history) : board_(board), history_(history), gamePhase_(0) {
    if constexpr (Flags & MoveOrdering::Flags::History) {
        assert(history != nullptr && "History flag is set but history table is null.");
    } else {
        assert(history == nullptr && "History flag is not set but history table is not null.");
    }

    this->gamePhase_ = TaperedEval::calculateContinuousPhase(board);
}



template<Color Side, uint32_t Flags>
int32_t MoveScorer<Side, Flags>::score(Move move) {
    int32_t score = 0;

    Board &board = this->board_;

    Piece piece = board.pieceAt(move.from());

    // If the move is a quiet move. This is needed because the quiet and tactical flags can be used together. If the tactical
    // flag is not used, then we can assume that the move is quiet, and GCC will optimize out the if statement.
    bool isQuiet = true;

    // Tactical moves
    if constexpr (Flags & MoveOrdering::Flags::Tactical) {
        // Captures
        if (move.isCapture()) {
            isQuiet = false;

            // Static exchange evaluation
            score += See::evaluate<Side>(move, board);
        }

        // Promotions
        if (move.isPromotion()) {
            score += PieceMaterial::value(move.promotion()) * 10;
        }
    }

    // History heuristics
    if constexpr (Flags & MoveOrdering::Flags::History) {
        if (isQuiet) {
            score += this->history().score(Side, piece.type(), move.to(), 5000);
        }
    }

    // Quiet moves
    if constexpr (Flags & MoveOrdering::Flags::Quiet) {
        if (isQuiet) {
            // Castling
            if (move.isCastle()) {
                score += 500;
            }

            // Piece square tables
            score += PieceSquareTables::interpolate(piece, move.to(), this->gamePhase_);
            score -= PieceSquareTables::interpolate(piece, move.from(), this->gamePhase_);
        }
    }

    return score;
}



template<Color Side, uint32_t Flags>
void MoveOrdering::score(MovePriorityQueue &moves, Board &board, const HistoryTable *history) {
    MoveScorer<Side, Flags> scorer(board, history);

    for (MoveEntry *entry = moves.start(); entry != moves.end(); ++entry) {
        entry->score = scorer.score(entry->move);
    }
}



template<Color Side, uint32_t Flags>
INLINE void scoreRoot(RootMoveList &moves, Board &board, const HistoryTable *history) {
    MoveScorer<Side, Flags> scorer(board, history);

    for (MoveEntry &entry : moves.moves()) {
        entry.score = scorer.score(entry.move);
    }
}

template<uint32_t Flags>
void MoveOrdering::score(RootMoveList &moves, Board &board, const HistoryTable *history) {
    if (board.turn() == Color::White) {
        scoreRoot<Color::White, Flags>(moves, board, history);
    } else {
        scoreRoot<Color::Black, Flags>(moves, board, history);
    }
}

// @formatter:off
template void MoveOrdering::score<Color::White, MoveOrdering::Type::Quiet>(MovePriorityQueue &, Board &, const HistoryTable *);
template void MoveOrdering::score<Color::Black, MoveOrdering::Type::Quiet>(MovePriorityQueue &, Board &, const HistoryTable *);
template void MoveOrdering::score<Color::White, MoveOrdering::Type::Tactical>(MovePriorityQueue &, Board &, const HistoryTable *);
template void MoveOrdering::score<Color::Black, MoveOrdering::Type::Tactical>(MovePriorityQueue &, Board &, const HistoryTable *);
template void MoveOrdering::score<Color::White, MoveOrdering::Type::All>(MovePriorityQueue &, Board &, const HistoryTable *);
template void MoveOrdering::score<Color::Black, MoveOrdering::Type::All>(MovePriorityQueue &, Board &, const HistoryTable *);
template void MoveOrdering::score<Color::White, MoveOrdering::Type::AllNoHistory>(MovePriorityQueue &, Board &, const HistoryTable *);
template void MoveOrdering::score<Color::Black, MoveOrdering::Type::AllNoHistory>(MovePriorityQueue &, Board &, const HistoryTable *);

template void MoveOrdering::score<MoveOrdering::Type::Quiet>(RootMoveList &, Board &, const HistoryTable *);
template void MoveOrdering::score<MoveOrdering::Type::Tactical>(RootMoveList &, Board &, const HistoryTable *);
template void MoveOrdering::score<MoveOrdering::Type::All>(RootMoveList &, Board &, const HistoryTable *);
template void MoveOrdering::score<MoveOrdering::Type::AllNoHistory>(RootMoveList &, Board &, const HistoryTable *);
// @formatter:on
