#include "fixed_search.h"

#include <vector>
#include <optional>

#include "score.h"
#include "engine/board/piece.h"
#include "engine/move/move.h"
#include "engine/move/move_list.h"
#include "engine/move/movegen.h"
#include "engine/move/legality_check.h"
#include "engine/eval/evaluation.h"

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
        bestLine.push_back(move);

        // We are only using the board's hash, so we don't need to update anything else
        board.makeMove<MakeMoveType::HashOnly>(move);

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

        int32_t score = -search<~Turn>(depth - 1, -INT32_MAX, -alpha);

        if (score > alpha) {
            bestMove = move;
            alpha = score;
        }

        board.unmakeMove<MakeMoveType::AllNoTurn>(move, info);
    }

    // Transposition table store
    table.store(board.hash(), depth, TranspositionTable::Flag::Exact, bestMove, alpha);

    return { bestMove, alpha };
}



template<Color Turn>
int32_t FixedDepthSearcher::searchQuiesce(int32_t alpha, int32_t beta) {
    this->stats_.incrementNodeCount();

    Board &board = this->board_;

    int32_t standPat = Evaluation::evaluate<Turn>(board, alpha, beta);
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
INLINE int32_t FixedDepthSearcher::searchAlphaBeta(Move &bestMove, Move hashMove, uint16_t depth, int32_t &alpha, int32_t beta) {
    if (depth == 0) {
        return this->searchQuiesce<Turn>(alpha, beta);
    }

    this->stats_.incrementNodeCount();

    Board &board = this->board_;

    // Stage 1: Check for repetition
    if (alpha < Score::Draw && board.isRepetition()) {
        return Score::Draw;
    }

    // Stage 2: Null move pruning
    bool isInCheck = board.isInCheck<Turn>();
    // TODO: Make this less risky (e.g. zugzwang detection?)
    if (depth >= 3 && !isInCheck) {
        MakeMoveInfo info = board.makeNullMove();

        // TODO: Is it safe to pass -beta + 1 as the beta parameter here?
        //  This is just what Copilot spit out, it seems to work fine but I'm not sure if it's correct. It definitely causes more
        //  pruning than just passing -alpha though.
        int32_t score = -this->search<~Turn>(depth - 3, -beta, -beta + 1);

        board.unmakeNullMove(info);

        if (score >= beta) {
            return beta;
        }
    }

    int32_t bestScore = -INT32_MAX;

    // Stage 3: Hash move
    //
    // Try the hash move first, if it exists. We can save all move generation entirely if the hash move causes a beta-cutoff.
    //
    // We also have to check the legality of the hash move in case of rare hash key collisions (see
    // https://www.chessprogramming.org/Transposition_Table#KeyCollisions).
    LegalityChecker<Turn> legalityChecker(board);

    if (hashMove.isValid() && legalityChecker.isLegal(hashMove)) {
        MakeMoveInfo info = board.makeMove<MakeMoveType::AllNoTurn>(hashMove);

        int32_t score = -this->search<~Turn>(depth - 1, -beta, -alpha);

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

    // Stage 4: Tactical move search
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

            int32_t score = -this->search<~Turn>(depth - 1, -beta, -alpha);

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

    // Stage 5: Killer moves
    for (Move killer : this->heuristics_.killers[depth]) {
        if (!killer.isValid() || !legalityChecker.isLegal(killer)) {
            continue;
        }

        MakeMoveInfo info = board.makeMove<MakeMoveType::AllNoTurn>(killer);

        int32_t score = -this->search<~Turn>(depth - 1, -beta, -alpha);

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

    // Stage 6: Quiet move search
    {
        MoveEntry *movesEnd = MoveGeneration::generate<Turn, MoveGeneration::Type::Quiet>(board, movesStart);

        MovePriorityQueue moves(movesStart, movesEnd);

        if (moves.empty() && !hasTacticalMoves) {
            if (isInCheck) { // Checkmate
                // TODO: If depth is reduced, the mate-in score will be incorrect
                return Score::mateIn(this->depth_ - depth);
            } else { // Stalemate
                return Score::Draw;
            }
        }

        // Futility pruning
        if (depth == 1 && !isInCheck) {
            constexpr int32_t FutilityMargin = 300;

            int32_t evaluation = Evaluation::evaluate<Turn>(board, alpha, beta);

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
        uint16_t nodeIndex = 0;

        while (!moves.empty()) {
            Move move = moves.dequeue();
            MakeMoveInfo info = board.makeMove<MakeMoveType::AllNoTurn>(move);

            uint16_t depthReduction = 0;
            if (depth >= 3 && !isInCheck && nodeIndex >= 4) {
                // Late move reduction
                depthReduction = 1;

                if (nodeIndex >= 10) {
                    depthReduction = (depth / 3);
                }
            }

            int32_t score = -this->search<~Turn>(depth - 1 - depthReduction, -beta, -alpha);

            if (score > bestScore) {
                bestScore = score;
                bestMove = move;

                if (score > alpha) {
                    if (depthReduction > 0) {
                        // If the move was above alpha, but we reduced the depth, we have to search the move again with the full
                        // depth.
                        score = -this->search<~Turn>(depth - 1, -beta, -alpha);

                        if (score > bestScore) {
                            bestScore = score;
                            bestMove = move;
                            alpha = std::max(alpha, score);
                        }
                    } else {
                        alpha = score;
                    }
                }
            }

            board.unmakeMove<MakeMoveType::AllNoTurn>(move, info);

            if (score >= beta) {
                this->heuristics_.history.add(Turn, board, move, depth);
                this->heuristics_.killers.add(depth, move);
                return bestScore;
            }

            nodeIndex++;
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
    int32_t score = this->searchAlphaBeta<Turn>(bestMove, hashMove, depth, alpha, beta);

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
        table.store(board.hash(), depth, flag, bestMove, score);
    }

    return score;
}
