#include "search.h"

#include <thread>
#include <vector>
#include <optional>

#include "move.h"
#include "movegen.h"
#include "movesort.h"

FixedDepthSearcher::FixedDepthSearcher(const Board &board, int32_t depth) : FixedDepthSearcher(board, depth,
    nullptr) { }

FixedDepthSearcher::FixedDepthSearcher(const Board &board, int32_t depth, FixedDepthSearcher *previousIteration)
    : board_(board.copy()), depth_(depth), moves_(depth, 64 * depth), table_(65536),
      previousIteration_(previousIteration) { }

int32_t evaluate(const Board &board) {
    return board.material(board.turn()) - board.material(~board.turn());
}

FixedDepthSearchResult FixedDepthSearcher::searchRoot() {
    Board &board = this->board_;
    int32_t depth = this->depth_;
    MoveListStack &moves = this->moves_;

    moves.push();
    MoveGenerator::generate(board, moves);

    std::optional<Move> bestMove = std::nullopt;
    int32_t bestScore = -INT32_MAX;
    uint32_t nodeCount = 0;
    uint32_t transpositionHits = 0;
    int32_t alpha = -INT32_MAX;

    for (uint32_t i = 0; i < moves.size(); ++i) {
        Move move = moves.unsafeAt(i);

        board.makeMove(move);

        SearchNode node = search(depth - 1, -INT32_MAX, -alpha);
        nodeCount += node.nodeCount;
        transpositionHits += node.transpositionHits;

        int32_t score = -node.score;
        if (score > bestScore) {
            bestMove = move;
            bestScore = score;
        }

        board.unmakeMove(move);

        alpha = std::max(alpha, score);
    }

    moves.pop();

    return { bestMove, bestScore, nodeCount, transpositionHits };
}

SearchNode FixedDepthSearcher::search(int32_t depth, int32_t alpha, int32_t beta) {
    Board &board = this->board_;
    MoveListStack &moves = this->moves_;
    TranspositionTable &table = this->table_;

    if (depth == 0) {
        return { evaluate(board), 1, 0 };
    }

    // Transposition table lookup
    {
        SpinLockGuard lock(table.lock());

        TranspositionTable::Entry *entry = table.load(board.hash());
        if (entry != nullptr && entry->depth >= depth) {
            if (entry->flag == TranspositionTable::Flag::Exact) {
                return { entry->bestScore, 0, 1 };
            } else if (entry->flag == TranspositionTable::Flag::LowerBound) {
                alpha = std::max(alpha, entry->bestScore);
            } else if (entry->flag == TranspositionTable::Flag::UpperBound) {
                beta = std::min(beta, entry->bestScore);
            }

            if (alpha >= beta) {
                return { entry->bestScore, 0, 1 };
            }
        }
    }

    int32_t originalAlpha = alpha;

    // Move search
    moves.push();
    MoveGenerator::generate(board, moves);

    if (moves.size() == 0) {
        moves.pop();
        // TODO: Stalemate
        return { -INT32_MAX + depth, 1, 0 };
    }

    MoveSort::sort(moves);

    std::optional<Move> bestMove = std::nullopt;
    int32_t bestScore = -INT32_MAX;
    uint32_t nodeCount = 0;
    uint32_t transpositionHits = 0;

    for (uint32_t i = 0; i < moves.size(); ++i) {
        Move move = moves.unsafeAt(i);
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

    moves.pop();

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

    return { bestScore, nodeCount, transpositionHits };
}
