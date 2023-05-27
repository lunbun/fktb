#include "fixed_search.h"

#include <vector>
#include <optional>

#include "piece.h"
#include "move.h"
#include "movegen.h"
#include "evaluation.h"

SearchNode SearchNode::invalid() {
    return { 0, 0, 0 };
}

SearchRootNode SearchRootNode::invalid() {
    return { Move::invalid(), 0, 0, 0 };
}

SearchResult SearchResult::invalid() {
    return { false, 0, { }, 0, 0, 0 };
}



FixedDepthSearcher::FixedDepthSearcher(const Board &board, uint16_t depth, TranspositionTable &table)
    : FixedDepthSearcher(board, depth, table, nullptr) { }

FixedDepthSearcher::FixedDepthSearcher(const Board &board, uint16_t depth, TranspositionTable &table,
    ReadonlyTranspositionTable *previousTable) : board_(board.copy()), depth_(depth), moves_(depth, 64 * depth),
                                                 table_(table), previousTable_(previousTable) { }

void FixedDepthSearcher::halt() {
    this->isHalted_ = true;
}



SearchResult FixedDepthSearcher::search() {
    SearchRootNode node = SearchRootNode::invalid();
    if (this->board_.turn() == Color::White) {
        node = this->searchRoot<Color::White>();
    } else {
        node = this->searchRoot<Color::Black>();
    }

    if (this->isHalted_) {
        return SearchResult::invalid();
    }

    // Find the best line using the transposition table
    std::vector<Move> bestLine;

    Board board = this->board_.copy();
    uint16_t depth = this->depth_;
    Move move = node.move;
    while (move.isValid()) {
        bestLine.push_back(move);

        // No need to update the turn since this is a copy of the board that is only used for finding the best line
        board.makeMoveNoTurnUpdate(move);

        // Find the next best move in the line
        depth--;
        TranspositionTable::Entry *entry = this->table_.load(board.hash());
        if (entry == nullptr || entry->depth() < depth || entry->flag() != TranspositionTable::Flag::Exact) {
            break;
        }
        move = entry->bestMove();
    }

    return { true, this->depth_, std::move(bestLine), node.score, node.nodeCount, node.transpositionHits };
}

template<Color Turn>
SearchRootNode FixedDepthSearcher::searchRoot() {
    if (this->isHalted_) {
        return SearchRootNode::invalid();
    }

    uint16_t depth = this->depth_;
    Board &board = this->board_;
    TranspositionTable &table = this->table_;

    // Move search
    MovePriorityQueueStack &moves = this->moves_;

    // Creating a MovePriorityQueueStackGuard pushes a stack frame onto the MovePriorityQueueStack
    MovePriorityQueueStackGuard movesStackGuard(moves);

    moves.maybeLoadHashMoveFromPreviousIteration(board, this->previousTable_);
    MoveGenerator<Turn> generator(board, moves);
    generator.generate();

    if (moves.empty()) {
        // TODO: Checkmate detection
        return { Move::invalid(), 0, 1, 0 };
    }

    moves.score<Turn>(board);

    Move bestMove = Move::invalid();
    int32_t alpha = -INT32_MAX; // In the root node, alpha is the same thing as the best score
    uint32_t nodeCount = 0;
    uint32_t transpositionHits = 0;

    while (!moves.empty()) {
        Move move = moves.dequeue();
        // No need to update the turn since we do that manually with templates
        MakeMoveInfo moveInfo = board.makeMoveNoTurnUpdate(move);

        SearchNode node = search<~Turn>(depth - 1, -INT32_MAX, -alpha);
        nodeCount += node.nodeCount;
        transpositionHits += node.transpositionHits;

        int32_t score = -node.score;
        if (score > alpha) {
            bestMove = move;
            alpha = score;
        }

        board.unmakeMoveNoTurnUpdate(move, moveInfo);
    }

    // Transposition table store
    {
        SpinLockGuard lock(table.lock());
        table.store(board.hash(), depth, TranspositionTable::Flag::Exact, bestMove, alpha);
    }

    return { bestMove, alpha, nodeCount, transpositionHits };
}



template<Color Turn>
INLINE SearchNode FixedDepthSearcher::searchNoTransposition(Move &bestMove, uint16_t depth, int32_t &alpha, int32_t beta) {
    Board &board = this->board_;

    if (depth == 0) {
        return { Evaluation::evaluate<Turn>(board), 1, 0 };
    }

    // Move search
    MovePriorityQueueStack &moves = this->moves_;

    // Creating a MovePriorityQueueStackGuard pushes a stack frame onto the MovePriorityQueueStack
    MovePriorityQueueStackGuard movesStackGuard(moves);

    moves.maybeLoadHashMoveFromPreviousIteration(board, this->previousTable_);
    MoveGenerator<Turn> generator(board, moves);
    generator.generate();

    if (moves.empty()) {
        // TODO: Checkmate detection
        return { 0, 1, 0 };
    }

    moves.score<Turn>(board);

    int32_t bestScore = -INT32_MAX;
    uint32_t nodeCount = 0;
    uint32_t transpositionHits = 0;

    while (!moves.empty()) {
        Move move = moves.dequeue();
        // No need to update the turn since we do that manually with templates
        MakeMoveInfo moveInfo = board.makeMoveNoTurnUpdate(move);

        SearchNode node = search<~Turn>(depth - 1, -beta, -alpha);
        nodeCount += node.nodeCount;
        transpositionHits += node.transpositionHits;

        int32_t score = -node.score;
        if (score > bestScore) {
            bestMove = move;
            bestScore = score;
        }

        board.unmakeMoveNoTurnUpdate(move, moveInfo);

        if (score >= beta) {
            break;
        }
        alpha = std::max(alpha, score);
    }

    return { bestScore, nodeCount, transpositionHits };
}

template<Color Turn>
SearchNode FixedDepthSearcher::search(uint16_t depth, int32_t alpha, int32_t beta) {
    if (this->isHalted_) {
        return SearchNode::invalid();
    }

    Board &board = this->board_;
    TranspositionTable &table = this->table_;

    // Transposition table lookup
    {
        SpinLockGuard lock(table.lock());

        TranspositionTable::Entry *entry = table.load(board.hash());
        if (entry != nullptr && entry->depth() >= depth) {
            if (entry->flag() == TranspositionTable::Flag::Exact) {
                return { entry->bestScore(), 0, 1 };
            } else if (entry->flag() == TranspositionTable::Flag::LowerBound) {
                alpha = std::max(alpha, entry->bestScore());
            } else if (entry->flag() == TranspositionTable::Flag::UpperBound) {
                beta = std::min(beta, entry->bestScore());
            }

            if (alpha >= beta) {
                return { entry->bestScore(), 0, 1 };
            }
        }
    }

    int32_t originalAlpha = alpha;

    Move bestMove = Move::invalid();
    SearchNode node = this->searchNoTransposition<Turn>(bestMove, depth, alpha, beta);

    // Transposition table store
    TranspositionTable::Flag flag;
    if (node.score <= originalAlpha) {
        flag = TranspositionTable::Flag::UpperBound;
    } else if (node.score >= beta) {
        flag = TranspositionTable::Flag::LowerBound;
    } else {
        flag = TranspositionTable::Flag::Exact;
    }
    {
        SpinLockGuard lock(table.lock());
        table.store(board.hash(), depth, flag, bestMove, node.score);
    }

    return node;
}
