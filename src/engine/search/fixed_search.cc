#include "fixed_search.h"

#include <vector>
#include <optional>

#include "score.h"
#include "evaluation.h"
#include "engine/board/piece.h"
#include "engine/move/move.h"
#include "engine/move/move_list.h"
#include "engine/move/movegen.h"

FixedDepthSearcher::FixedDepthSearcher(const Board &board, uint16_t depth, TranspositionTable &table,
    SearchDebugInfo &debugInfo) : board_(board.copy()), depth_(depth), table_(table), debugInfo_(debugInfo) { }

void FixedDepthSearcher::halt() {
    this->isHalted_ = true;
}



SearchLine FixedDepthSearcher::search() {
    RootMoveList moves = MoveGeneration::generateLegalRoot(this->board_);

    moves.score(this->board_);
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

    this->debugInfo_.incrementNodeCount();

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
        // No need to update the turn since we do that manually with templates
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
    this->debugInfo_.incrementNodeCount();

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

    MoveEntry *movesEnd = MoveGeneration::generateLegal<Turn, true>(board, movesStart);

    MovePriorityQueue moves(movesStart, movesEnd);
    moves.score<Turn>(board);

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
INLINE int32_t FixedDepthSearcher::searchNoTransposition(Move &bestMove, uint16_t depth, int32_t &alpha, int32_t beta) {
    this->debugInfo_.incrementNodeCount();

    Board &board = this->board_;

    if (depth == 0) {
        return this->searchQuiesce<Turn>(alpha, beta);
    }

    // Move generation and scoring
    AlignedMoveEntry moveBuffer[MaxMoveCount];
    MoveEntry *movesStart = MoveEntry::fromAligned(moveBuffer);

    MoveEntry *movesEnd = MoveGeneration::generateLegal<Turn, false>(board, movesStart);

    MovePriorityQueue moves(movesStart, movesEnd);
    moves.loadHashMove(board, this->table_);

    if (moves.empty()) {
        if (board.isInCheck<Turn>()) { // Checkmate
            return Score::mateIn(this->depth_ - depth);
        } else { // Stalemate
            return 0;
        }
    }

    moves.score<Turn>(board);

    // Search
    int32_t bestScore = -INT32_MAX;

    while (!moves.empty()) {
        Move move = moves.dequeue();
        // No need to update the turn since we do that manually with templates
        MakeMoveInfo moveInfo = board.makeMove<false>(move);

        int32_t score = -this->search<~Turn>(depth - 1, -beta, -alpha);

        if (score > bestScore) {
            bestMove = move;
            bestScore = score;
        }

        board.unmakeMove<false>(move, moveInfo);

        if (score >= beta) {
            break;
        }
        alpha = std::max(alpha, score);
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
    {
        TranspositionTable::LockedEntry entry = table.load(board.hash());
        if (entry != nullptr && entry->depth() >= depth) {
            this->debugInfo_.incrementTranspositionHits();

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

    int32_t originalAlpha = alpha;

    Move bestMove = Move::invalid();
    int32_t score = this->searchNoTransposition<Turn>(bestMove, depth, alpha, beta);

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
