#include "fixed_search.h"

#include <vector>
#include <optional>

#include "piece.h"
#include "move.h"
#include "movegen.h"

SearchNode SearchNode::invalid() {
    return { std::nullopt, 0, 0, 0 };
}

SearchResult SearchResult::invalid() {
    return { false, 0, {}, 0, 0, 0 };
}



FixedDepthSearcher::FixedDepthSearcher(const Board &board, uint16_t depth) : FixedDepthSearcher(board, depth,
    nullptr) { }

FixedDepthSearcher::FixedDepthSearcher(const Board &board, uint16_t depth, FixedDepthSearcher *previousIteration)
    : board_(board.copy()), depth_(depth), moves_(depth, 64 * depth), table_(1048576),
      previousIteration_(previousIteration) { }

void FixedDepthSearcher::halt() {
    this->isHalted_ = true;
}



inline int32_t evaluate(const Board &board, PieceColor color) {
    int32_t score = 0;

    score += board.material(color);

    // Bonus for having a bishop pair
    score += PieceMaterial::BishopPair * (board.bishops()[color].size() >= 2);

    return score;
}

inline int32_t evaluate(const Board &board) {
    return evaluate(board, board.turn()) - evaluate(board, ~board.turn());
}

SearchResult FixedDepthSearcher::searchRoot() {
    SearchNode node = this->search(this->depth_, -INT32_MAX, INT32_MAX);

    if (this->isHalted_) {
        return SearchResult::invalid();
    }

    // Find the best line using the transposition table
    std::vector<Move> bestLine;

    Board board = this->board_.copy();
    uint16_t depth = this->depth_;
    std::optional<Move> move = node.move;
    while (move.has_value()) {
        bestLine.push_back(move.value());

        board.makeMove(move.value());

        // Find the next best move in the line
        depth--;
        TranspositionTable::Entry *entry = this->table_.load(board.hash());
        if (entry == nullptr || entry->depth < depth || entry->flag != TranspositionTable::Flag::Exact) {
            break;
        }
        move = entry->bestMove;
    }

    return { true, this->depth_, std::move(bestLine), node.score, node.nodeCount, node.transpositionHits };
}

SearchNode FixedDepthSearcher::search(uint16_t depth, int32_t alpha, int32_t beta) {
    if (this->isHalted_) {
        return SearchNode::invalid();
    }

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
        // TODO: Checkmate detection
        return { std::nullopt, 0, 1, 0 };
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
