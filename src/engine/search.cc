#include "search.h"

#include <thread>
#include <vector>
#include <optional>

#include "move.h"
#include "movegen.h"

FixedDepthSearcher::FixedDepthSearcher(const Board &board, int32_t depth) : FixedDepthSearcher(board, depth,
    nullptr) { }

FixedDepthSearcher::FixedDepthSearcher(const Board &board, int32_t depth, FixedDepthSearcher *previousIteration)
    : board_(board.copy()), depth_(depth), moves_(depth, 64 * depth), table_(524288),
      previousIteration_(previousIteration) { }

int32_t evaluate(const Board &board) {
    return board.material(board.turn()) - board.material(~board.turn());
}

SearchNode FixedDepthSearcher::searchRoot() {
    return this->search(this->depth_, -INT32_MAX, INT32_MAX);
}

SearchNode FixedDepthSearcher::search(int32_t depth, int32_t alpha, int32_t beta) {
    Board &board = this->board_;
    TranspositionTable &table = this->table_;

    if (depth == 0) {
        return { std::nullopt, evaluate(board), 1, 0 };
    }

    // Transposition table lookup
    {
        SpinLockGuard lock(table.lock());

        TranspositionTable::Entry *entry = table.load(board.hash());
        if (entry != nullptr && entry->depth >= depth) {
            if (entry->flag == TranspositionTable::Flag::Exact) {
                return { entry->bestMove, entry->bestScore, 0, 1 };
            } else if (entry->flag == TranspositionTable::Flag::LowerBound) {
                alpha = std::max(alpha, entry->bestScore);
            } else if (entry->flag == TranspositionTable::Flag::UpperBound) {
                beta = std::min(beta, entry->bestScore);
            }

            if (alpha >= beta) {
                return { entry->bestMove, entry->bestScore, 0, 1 };
            }
        }
    }

    int32_t originalAlpha = alpha;

    // Move search
    MovePriorityQueueStack &moves = this->moves_;

    // Creating a MovePriorityQueueStackGuard pushes a stack frame onto the MovePriorityQueueStack
    MovePriorityQueueStackGuard movesStackGuard(moves);

    moves.maybeLoadHashMoveFromPreviousIteration(board, this->previousIteration_);
    MoveGenerator::generate(board, moves);

    if (moves.empty()) {
        // TODO: Stalemate
        return { std::nullopt, -INT32_MAX + depth, 1, 0 };
    }

    std::optional<Move> bestMove = std::nullopt;
    int32_t bestScore = -INT32_MAX;
    uint32_t nodeCount = 0;
    uint32_t transpositionHits = 0;

    while (!moves.empty()) {
        Move move = moves.dequeue();
        board.makeMove(move);

        SearchNode node = search(depth - 1, -beta, -alpha);
        nodeCount += node.nodeCount;
        transpositionHits += node.transpositionHits;

        int32_t score = -node.score;
        if (score > bestScore) {
            bestMove = move;
            bestScore = score;
        }

        board.unmakeMove(move);

        if (alpha > beta) {
            break;
        }
        alpha = std::max(alpha, score);
    }

    // Transposition table store
    TranspositionTable::Flag flag;
    if (bestScore <= originalAlpha) {
        flag = TranspositionTable::Flag::UpperBound;
    } else if (bestScore >= beta) {
        flag = TranspositionTable::Flag::LowerBound;
    } else {
        flag = TranspositionTable::Flag::Exact;
    }
    {
        SpinLockGuard lock(table.lock());
        table.store(board.hash(), depth, flag, bestMove, bestScore);
    }

    return { bestMove, bestScore, nodeCount, transpositionHits };
}
