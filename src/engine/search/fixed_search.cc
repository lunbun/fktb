#include "fixed_search.h"

#include <vector>
#include <optional>

#include "score.h"
#include "engine/board/piece.h"
#include "engine/move/move.h"
#include "engine/move/move_list.h"
#include "engine/move/movegen.h"
#include "engine/eval/evaluation.h"

FixedDepthSearcher::FixedDepthSearcher(const Board &board, uint16_t depth, TranspositionTable &table, HistoryTable &history,
    SearchStatistics &stats) : board_(board.copy()), depth_(depth), table_(table), history_(history), stats_(stats) { }

void FixedDepthSearcher::halt() {
    this->isHalted_ = true;
}



SearchLine FixedDepthSearcher::search() {
    RootMoveList moves = MoveGeneration::generateLegalRoot(this->board_);

    MoveOrdering::score<MoveOrdering::Type::History>(moves, this->board_, &this->history_);
    moves.sort();
    moves.loadHashMove(this->board_, this->table_);

    return this->search(std::move(moves));
}

SearchLine FixedDepthSearcher::search(RootMoveList moves) {
    SearchRootNode node = SearchRootNode::invalid();
    if (this->board_.turn() == Color::White) {
        node = this->searchRoot<Color::White>(std::move(moves));
    } else {
        node = this->searchRoot<Color::Black>(std::move(moves));
    }

    if (this->isHalted_) {
        return SearchLine::invalid();
    }

    // Find the best line using the transposition table
    std::vector<Move> bestLine;

    Board board = this->board_.copy();
    uint16_t depth = this->depth_;
    Move move = node.move;
    while (move.isValid()) {
        bestLine.push_back(move);

        // No need to update the turn since this is a copy of the board that is only used for finding the best line
        board.makeMove<false>(move);

        // Find the next best move in the line
        depth--;

        // Do not allow depth to go below 0, since the best line might just be shuffling back and forth, which would
        // make this go into an infinite loop
        if (depth == 0) {
            break;
        }

        TranspositionTable::LockedEntry entry = this->table_.load(board.hash());
        if (entry == nullptr || entry->depth() < depth || entry->flag() != TranspositionTable::Flag::Exact) {
            break;
        }
        move = entry->bestMove();
    }

    return { std::move(bestLine), node.score };
}

template<Color Turn>
SearchRootNode FixedDepthSearcher::searchRoot(RootMoveList moves) {
    if (this->isHalted_) {
        return SearchRootNode::invalid();
    }

    this->stats_.incrementNodeCount();

    uint16_t depth = this->depth_;
    Board &board = this->board_;
    TranspositionTable &table = this->table_;

    if (moves.empty()) {
        if (board.isInCheck<Turn>()) { // Checkmate
            return { Move::invalid(), Score::mateIn(0) };
        } else { // Stalemate
            return { Move::invalid(), 0 };
        }
    }

    // Search
    Move bestMove = Move::invalid();
    int32_t alpha = -INT32_MAX;

    while (!moves.empty()) {
        Move move = moves.dequeue();
        MakeMoveInfo moveInfo = board.makeMove<false>(move);

        int32_t score = -search<~Turn>(depth - 1, -INT32_MAX, -alpha);

        if (score > alpha) {
            bestMove = move;
            alpha = score;
        }

        board.unmakeMove<false>(move, moveInfo);
    }

    // Transposition table store
    table.store(board.hash(), depth, TranspositionTable::Flag::Exact, bestMove, alpha);

    return { bestMove, alpha };
}



template<Color Turn>
int32_t FixedDepthSearcher::searchQuiesce(int32_t alpha, int32_t beta) {
    this->stats_.incrementNodeCount();

    Board &board = this->board_;

    int32_t standPat = Evaluation::evaluate<Turn>(board);
    if (standPat >= beta) {
        return beta;
    }

    // Delta pruning
    int32_t delta = (PieceMaterial::Queen + 200);
    if (standPat + delta <= alpha) {
        return alpha;
    }

    alpha = std::max(alpha, standPat);

    // Move generation and scoring
    AlignedMoveEntry moveBuffer[MaxCaptureCount];
    MoveEntry *movesStart = MoveEntry::fromAligned(moveBuffer);

    MoveEntry *movesEnd = MoveGeneration::generate<Turn, MoveGeneration::Type::Tactical>(board, movesStart);

    MovePriorityQueue moves(movesStart, movesEnd);
    MoveOrdering::score<Turn, MoveOrdering::Type::NoHistory>(moves, board, nullptr);

    // Capture search
    while (!moves.empty()) {
        Move move = moves.dequeue();
        // No need to update the turn since we do that manually with templates
        MakeMoveInfo moveInfo = board.makeMove<false>(move);

        int32_t score = -this->searchQuiesce<~Turn>(-beta, -alpha);

        board.unmakeMove<false>(move, moveInfo);

        if (score >= beta) {
            return beta;
        }

        alpha = std::max(alpha, score);
    }

    return alpha;
}

template<Color Turn>
INLINE int32_t FixedDepthSearcher::searchAlphaBeta(Move &bestMove, Move hashMove, uint16_t depth, int32_t &alpha, int32_t beta) {
    if (depth == 0) {
        return this->searchQuiesce<Turn>(alpha, beta);
    }

    this->stats_.incrementNodeCount();

    Board &board = this->board_;

    int32_t bestScore = -INT32_MAX;

    // Try the hash move first, if it exists. We can save all move generation entirely if the hash move causes a beta-cutoff.
    if (hashMove.isValid()) {
        MakeMoveInfo moveInfo = board.makeMove<false>(hashMove);

        int32_t score = -this->search<~Turn>(depth - 1, -beta, -alpha);

        board.unmakeMove<false>(hashMove, moveInfo);

        if (score > bestScore) {
            bestScore = score;
            bestMove = hashMove;
            alpha = std::max(alpha, score);
        }

        if (score >= beta) {
            if (!hashMove.isCapture()) {
                // Update history heuristic
                this->history_.add<Turn>(board.pieceAt(hashMove.from()).type(), hashMove.to(), depth);
            }

            return bestScore;
        }
    }

    // Move generation and scoring
    AlignedMoveEntry moveBuffer[MaxMoveCount];
    MoveEntry *movesStart = MoveEntry::fromAligned(moveBuffer);

    MoveEntry *movesEnd = MoveGeneration::generate<Turn, MoveGeneration::Type::Legal>(board, movesStart);

    MovePriorityQueue moves(movesStart, movesEnd);

    if (moves.empty()) {
        if (board.isInCheck<Turn>()) { // Checkmate
            return Score::mateIn(this->depth_ - depth);
        } else { // Stalemate
            return 0;
        }
    }

    // We already tried the hash move, so remove it from the list of moves to search
    moves.remove(hashMove);

    MoveOrdering::score<Turn, MoveOrdering::Type::History>(moves, board, &this->history_);

    while (!moves.empty()) {
        Move move = moves.dequeue();
        MakeMoveInfo moveInfo = board.makeMove<false>(move);

        int32_t score = -this->search<~Turn>(depth - 1, -beta, -alpha);

        board.unmakeMove<false>(move, moveInfo);

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
            alpha = std::max(alpha, score);
        }

        if (score >= beta) {
            if (!move.isCapture()) {
                // Update history heuristic
                this->history_.add<Turn>(board.pieceAt(move.from()).type(), move.to(), depth);
            }

            return bestScore;
        }
    }

    return bestScore;
}

template<Color Turn>
int32_t FixedDepthSearcher::search(uint16_t depth, int32_t alpha, int32_t beta) {
    if (this->isHalted_) {
        return 0;
    }

    Board &board = this->board_;
    TranspositionTable &table = this->table_;

    // Transposition table lookup
    Move hashMove = Move::invalid();
    {
        TranspositionTable::LockedEntry entry = table.load(board.hash());
        if (entry != nullptr) {
            hashMove = entry->bestMove();

            if (entry->depth() >= depth) {
                this->stats_.incrementTranspositionHits();

                if (entry->flag() == TranspositionTable::Flag::Exact) {
                    return entry->bestScore();
                } else if (entry->flag() == TranspositionTable::Flag::LowerBound) {
                    alpha = std::max(alpha, entry->bestScore());
                } else if (entry->flag() == TranspositionTable::Flag::UpperBound) {
                    beta = std::min(beta, entry->bestScore());
                }

                if (alpha >= beta) {
                    return entry->bestScore();
                }
            }
        }
    }

    int32_t originalAlpha = alpha;

    Move bestMove = Move::invalid();
    int32_t score = this->searchAlphaBeta<Turn>(bestMove, hashMove, depth, alpha, beta);

    // Transposition table store
    TranspositionTable::Flag flag;
    if (score <= originalAlpha) {
        flag = TranspositionTable::Flag::UpperBound;
    } else if (score >= beta) {
        flag = TranspositionTable::Flag::LowerBound;
    } else {
        flag = TranspositionTable::Flag::Exact;
    }
    table.store(board.hash(), depth, flag, bestMove, score);

    return score;
}
