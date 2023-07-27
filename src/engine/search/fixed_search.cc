#include "fixed_search.h"

#include <vector>
#include <optional>

#include "score.h"
#include "engine/board/piece.h"
#include "engine/move/move.h"
#include "engine/move/move_list.h"
#include "engine/move/movegen.h"
#include "engine/move/legality_check.h"
#include "engine/eval/nnue.h"

namespace FKTB {

FixedDepthSearcher::FixedDepthSearcher(const Board &board, uint16_t depth, TranspositionTable &table, HeuristicTables &heuristics,
    SearchStatistics &stats) : board_(board.copy()), depth_(depth), table_(table), heuristics_(heuristics), stats_(stats) { }

void FixedDepthSearcher::halt() {
    this->isHalted_ = true;
}



SearchLine FixedDepthSearcher::search() {
    RootMoveList moves = MoveGeneration::generateLegalRoot(this->board_);

    MoveOrdering::score<MoveOrdering::Type::All>(moves, this->board_, &this->heuristics_.history);
    moves.sort();
    moves.loadHashMove(this->board_, this->table_);

    return this->search(std::move(moves));
}

SearchLine FixedDepthSearcher::search(RootMoveList moves) {
    // Resize the killer table to the depth of the search
    this->heuristics_.killers.resize(this->depth_);

    // Refresh the NNUE accumulator to alleviate any floating point errors that may have accumulated
    this->board_.accumulator().refresh(this->board_);

    // Search the root node
    SearchRootNode node = SearchRootNode::invalid();
    if (this->board_.turn() == Color::White) {
        node = this->searchRoot<Color::White>(std::move(moves));
    } else {
        node = this->searchRoot<Color::Black>(std::move(moves));
    }

    // Check if the search was halted
    if (this->isHalted_) {
        return SearchLine::invalid();
    }

    // Find the best line using the transposition table
    std::vector<Move> bestLine;

    Board board = this->board_.copy();
    uint16_t depth = this->depth_;
    Move move = node.move;
    while (move.isValid()) {
        // We have to check that the PV move is legal in case of rare hash key collisions (see
        // https://www.chessprogramming.org/Transposition_Table#KeyCollisions)
        if (!LegalityCheck::isLegal(board, move)) {
            break;
        }

        bestLine.push_back(move);

        board.makeMove<MakeMoveType::All>(move);

        // Find the next best move in the line
        depth--;

        // Do not allow depth to go below 0, since the best line might just be shuffling back and forth, which would
        // make this go into an infinite loop
        if (depth == 0) {
            break;
        }

        TranspositionTable::Entry *entry = this->table_.load(board.hash());
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
        MakeMoveInfo info = board.makeMove<MakeMoveType::AllNoTurn>(move);

        int32_t score = -search<~Turn>(depth - 1, 1, -INT32_MAX, -alpha);

        if (score > alpha) {
            bestMove = move;
            alpha = score;
        }

        board.unmakeMove<MakeMoveType::AllNoTurn>(move, info);
    }

    // Transposition table store
    table.maybeStore(board.hash(), depth, TranspositionTable::Flag::Exact, bestMove, alpha);

    return { bestMove, alpha };
}



template<Color Turn>
int32_t FixedDepthSearcher::searchQuiesce(int32_t alpha, int32_t beta) {
    this->stats_.incrementNodeCount();

    Board &board = this->board_;

    int32_t standPat = NNUE::evaluate(Turn, board.accumulator());
    if (standPat >= beta) {
        return beta;
    }

    // Delta pruning
    constexpr int32_t Delta = 1100;
    if (standPat + Delta < alpha) {
        return alpha;
    }

    alpha = std::max(alpha, standPat);

    // Move generation and scoring
    AlignedMoveEntry moveBuffer[MaxTacticalCount];
    MoveEntry *movesStart = MoveEntry::fromAligned(moveBuffer);

    MoveEntry *movesEnd = MoveGeneration::generate<Turn, MoveGeneration::Type::Tactical>(board, movesStart);

    MovePriorityQueue moves(movesStart, movesEnd);
    MoveOrdering::score<Turn, MoveOrdering::Type::Tactical>(moves, board, nullptr);

    // Capture search
    while (!moves.empty()) {
        Move move = moves.dequeue();
        MakeMoveInfo info = board.makeMove<MakeMoveType::AllNoTurn>(move);

        int32_t score = -this->searchQuiesce<~Turn>(-beta, -alpha);

        board.unmakeMove<MakeMoveType::AllNoTurn>(move, info);

        if (score >= beta) {
            return beta;
        }

        alpha = std::max(alpha, score);
    }

    return alpha;
}

template<Color Turn>
INLINE int32_t FixedDepthSearcher::searchAlphaBeta(Move &bestMove, Move hashMove, uint16_t depth, uint16_t ply, int32_t &alpha, int32_t beta) {
    if (depth == 0) {
        return this->searchQuiesce<Turn>(alpha, beta);
    }

    this->stats_.incrementNodeCount();

    Board &board = this->board_;

    // Stage 1: Null move pruning
    bool isInCheck = board.isInCheck<Turn>();
    if (depth >= 3 && !isInCheck) {
        MakeMoveInfo info = board.makeNullMove();

        // Pass -beta + 1 as alpha since it is a null window search (see https://www.chessprogramming.org/Null_Window).
        int32_t score = -this->search<~Turn>(depth - 3, ply + 1, -beta, -beta + 1);

        board.unmakeNullMove(info);

        if (score >= beta) {
            return beta;
        }
    }

    int32_t bestScore = -INT32_MAX;

    // Stage 2: Hash move
    //
    // Try the hash move first, if it exists. We can save all move generation entirely if the hash move causes a beta-cutoff.
    //
    // We also have to check the legality of the hash move in case of rare hash key collisions (see
    // https://www.chessprogramming.org/Transposition_Table#KeyCollisions).
    LegalityChecker<Turn> legalityChecker(board);

    if (hashMove.isValid() && legalityChecker.isLegal(hashMove)) {
        MakeMoveInfo info = board.makeMove<MakeMoveType::AllNoTurn>(hashMove);

        int32_t score = -this->search<~Turn>(depth - 1, ply + 1, -beta, -alpha);

        board.unmakeMove<MakeMoveType::AllNoTurn>(hashMove, info);

        if (score > bestScore) {
            bestScore = score;
            bestMove = hashMove;
            alpha = std::max(alpha, score);
        }

        if (score >= beta) {
            if (hashMove.isQuiet()) {
                this->heuristics_.history.add(Turn, board, hashMove, depth);
                this->heuristics_.killers.add(depth, hashMove);
            }

            return bestScore;
        }
    }

    // Stage 3: Tactical move search
    AlignedMoveEntry moveBuffer[MaxMoveCount];
    MoveEntry *movesStart = MoveEntry::fromAligned(moveBuffer);

    bool hasTacticalMoves;

    {
        MoveEntry *movesEnd = MoveGeneration::generate<Turn, MoveGeneration::Type::Tactical>(board, movesStart);

        MovePriorityQueue moves(movesStart, movesEnd);

        hasTacticalMoves = !moves.empty();

        // We already tried the hash move, so remove it from the list of moves to search
        if (hashMove.isValid() && hashMove.isTactical()) {
            moves.remove(hashMove);
        }

        MoveOrdering::score<Turn, MoveOrdering::Type::Tactical>(moves, board, nullptr);

        // Search all tactical moves
        while (!moves.empty()) {
            Move move = moves.dequeue();
            MakeMoveInfo info = board.makeMove<MakeMoveType::AllNoTurn>(move);

            int32_t score = -this->search<~Turn>(depth - 1, ply + 1, -beta, -alpha);

            board.unmakeMove<MakeMoveType::AllNoTurn>(move, info);

            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
                alpha = std::max(alpha, score);
            }

            if (score >= beta) {
                return bestScore;
            }
        }
    }

    // Stage 4: Killer moves
    for (Move killer : this->heuristics_.killers[depth]) {
        // We already tried the hash move, so skip it if it is also a killer move.
        //
        // We also have to check if the killer move is legal, since killers are just moves that caused a beta-cutoff in a sibling
        // node (or any node on the same ply in general), so it is possible that the killer move is not legal.
        if (!killer.isValid() || killer == hashMove || !legalityChecker.isLegal(killer)) {
            continue;
        }

        MakeMoveInfo info = board.makeMove<MakeMoveType::AllNoTurn>(killer);

        int32_t score = -this->search<~Turn>(depth - 1, ply + 1, -beta, -alpha);

        board.unmakeMove<MakeMoveType::AllNoTurn>(killer, info);

        if (score > bestScore) {
            bestScore = score;
            bestMove = killer;
            alpha = std::max(alpha, score);
        }

        if (score >= beta) {
            this->heuristics_.history.add(Turn, board, killer, depth);
            return bestScore;
        }
    }

    // Stage 5: Quiet move search
    {
        MoveEntry *movesEnd = MoveGeneration::generate<Turn, MoveGeneration::Type::Quiet>(board, movesStart);

        MovePriorityQueue moves(movesStart, movesEnd);

        if (moves.empty() && !hasTacticalMoves) {
            if (isInCheck) { // Checkmate
                return Score::mateIn(ply);
            } else { // Stalemate
                return Score::Draw;
            }
        }

        // Futility pruning
        if (depth == 1 && !isInCheck) {
            constexpr int32_t FutilityMargin = 300;

            int32_t evaluation = NNUE::evaluate(Turn, board.accumulator());

            if (evaluation + FutilityMargin <= alpha) {
                return alpha;
            }
        }

        // We already searched the hash move and killer moves, so remove them from the list of moves to search
        if (hashMove.isValid() && hashMove.isQuiet()) {
            moves.remove(hashMove);
        }
        for (Move killer : this->heuristics_.killers[depth]) {
            if (killer.isValid()) {
                moves.remove(killer);
            }
        }

        MoveOrdering::score<Turn, MoveOrdering::Type::Quiet>(moves, board, &this->heuristics_.history);

        // Search all quiet moves
        uint16_t moveIndex = 0;

        while (!moves.empty()) {
            Move move = moves.dequeue();
            MakeMoveInfo info = board.makeMove<MakeMoveType::AllNoTurn>(move);

            uint16_t depthReduction = 0;
            if (depth >= 3 && !isInCheck && moveIndex >= 4) {
                // Late move reduction
                depthReduction = 1;

                if (moveIndex >= 10) {
                    depthReduction = (depth / 3);
                }
            }

            int32_t score = -this->search<~Turn>(depth - 1 - depthReduction, ply + 1, -beta, -alpha);

            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
                alpha = std::max(alpha, score);
            }

            board.unmakeMove<MakeMoveType::AllNoTurn>(move, info);

            if (score >= beta) {
                this->heuristics_.history.add(Turn, board, move, depth);
                this->heuristics_.killers.add(depth, move);
                return bestScore;
            }

            moveIndex++;
        }
    }

    return bestScore;
}

template<Color Turn>
int32_t FixedDepthSearcher::search(uint16_t depth, uint16_t ply, int32_t alpha, int32_t beta) {
    if (this->isHalted_) {
        return 0;
    }

    Board &board = this->board_;

    // TODO: I would rather have threefold instead of twofold repetition for correctness, but I can't figure out how to solve the
    //  problem of the transposition table causing an early return on the second repetition, thereby forgoing the third repetition.
    // Check for repetition (needs to be done before transposition table lookup because if it's done after, we might return early
    // due to transposition, and it will not consider repetitions)
    if (board.isTwofoldRepetition()) {
        return Score::Draw;
    }

    TranspositionTable &table = this->table_;

    // Transposition table lookup
    Move hashMove = Move::invalid();
    TranspositionTable::Entry *entry = table.load(board.hash());
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

    int32_t originalAlpha = alpha;

    Move bestMove = Move::invalid();
    int32_t score = this->searchAlphaBeta<Turn>(bestMove, hashMove, depth, ply, alpha, beta);

    // Transposition table store
    if (bestMove.isValid()) {
        TranspositionTable::Flag flag;
        if (score <= originalAlpha) {
            flag = TranspositionTable::Flag::UpperBound;
        } else if (score >= beta) {
            flag = TranspositionTable::Flag::LowerBound;
        } else {
            flag = TranspositionTable::Flag::Exact;
        }
        table.maybeStore(board.hash(), depth, flag, bestMove, score);
    }

    return score;
}

} // namespace FKTB
