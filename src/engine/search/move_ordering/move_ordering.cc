#include "move_ordering.h"

#include <cassert>

#include "engine/move/move.h"
#include "engine/move/move_list.h"
#include "engine/board/piece.h"
#include "engine/board/board.h"
#include "engine/board/bitboard.h"
#include "engine/eval/piece_square_table.h"

// Class is used for convenience so that we don't have to pass around the board, history table, and bitboards separately as
// parameters.
template<Color Side, uint32_t Flags>
class MoveScorer {
public:
    explicit MoveScorer(const Board &board, const HistoryTable *history);

    [[nodiscard]] int32_t score(Move move);

private:
    const Board &board_;
    const HistoryTable *history_;

    Bitboard friendlyPawnAttacks_;
    Bitboard friendlyKnightAttacks_;

    Bitboard enemyKnights_;
    Bitboard enemyBishops_;
    Bitboard enemyRooks_;
    Bitboard enemyQueens_;

    Bitboard enemyPawnAttacks_;
    Bitboard enemyKnightAttacks_;
    Bitboard enemyBishopAttacks_;

    Bitboard enemyRookOrHigher_; // Rooks, queens, or kings
    Bitboard enemyBishopOrLowerAttacks_; // Bishop, knight, or pawn attacks
    Bitboard enemyRookOrLowerAttacks_; // Rook, bishop, knight, or pawn attacks
    Bitboard enemyQueenOrLowerAttacks_; // Queen, rook, bishop, knight, or pawn attacks

    [[nodiscard]] INLINE const HistoryTable &history() const {
        static_assert(Flags & MoveOrdering::Flags::History, "History flag is not set.");
        return *this->history_;
    }
};

template<Color Side, uint32_t Flags>
MoveScorer<Side, Flags>::MoveScorer(const Board &board, const HistoryTable *history) : board_(board), history_(history) {
    constexpr Color Enemy = ~Side;

    if constexpr (Flags & MoveOrdering::Flags::History) {
        assert(history != nullptr && "History flag is set but history table is null.");
    } else {
        assert(history == nullptr && "History flag is not set but history table is not null.");
    }

    Bitboard occupied = board.occupied();

    this->friendlyPawnAttacks_ = Bitboards::allPawnAttacks<Side>(board.bitboard(Piece::pawn(Side)));
    this->friendlyKnightAttacks_ = Bitboards::allKnightAttacks(board.bitboard(Piece::knight(Side)));

    this->enemyKnights_ = board.bitboard(Piece::knight(Enemy));
    this->enemyBishops_ = board.bitboard(Piece::bishop(Enemy));
    this->enemyRooks_ = board.bitboard(Piece::rook(Enemy));
    this->enemyQueens_ = board.bitboard(Piece::queen(Enemy));
    Bitboard enemyKing = (1ULL << board.king(Enemy));

    this->enemyPawnAttacks_ = Bitboards::allPawnAttacks<Enemy>(board.bitboard(Piece::pawn(Enemy)));
    this->enemyKnightAttacks_ = Bitboards::allKnightAttacks(this->enemyKnights_);
    this->enemyBishopAttacks_ = Bitboards::allBishopAttacks(this->enemyBishops_, occupied);
    Bitboard enemyRookAttacks = Bitboards::allRookAttacks(this->enemyRooks_, occupied);
    Bitboard enemyQueenAttacks = Bitboards::allQueenAttacks(this->enemyQueens_, occupied);

    this->enemyRookOrHigher_ = this->enemyRooks_ | this->enemyQueens_ | enemyKing;
    this->enemyBishopOrLowerAttacks_ = this->enemyBishopAttacks_ | this->enemyKnightAttacks_ | this->enemyPawnAttacks_;
    this->enemyRookOrLowerAttacks_ = enemyRookAttacks | this->enemyBishopOrLowerAttacks_;
    this->enemyQueenOrLowerAttacks_ = enemyQueenAttacks | this->enemyRookOrLowerAttacks_;
}

template<Color Side, uint32_t Flags>
int32_t MoveScorer<Side, Flags>::score(Move move) {
    int32_t score = 0;

    const Board &board = this->board_;

    Piece piece = board.pieceAt(move.from());

    // Castling
    if (move.isCastle()) {
        score += 1000;
    }

    // Captures
    if (move.isCapture()) {
        Piece captured = board.pieceAt(move.to());
        score += (captured.material() * 10 - piece.material());
    } else {
        // History heuristics
        if constexpr (Flags & MoveOrdering::Flags::History) {
            score += this->history().score(Side, piece.type(), move.to(), 5000);
        }
    }

    // Promotions
    if (move.isPromotion()) {
        score += (PieceMaterial::material(move.promotion()) * 10);
    }

    switch (piece.type()) {
        // Pawn heuristics
        case PieceType::Pawn: {
            // Use piece-square tables
            constexpr const auto &Table = PieceSquareTables::Pawn[Side];
            score += (Table[move.to()] * 10);

            // The pawn attacks after this move
            Bitboard pawnAttacks = Bitboards::pawnAttacks<Side>(move.to());

            // Attacking an enemy knight or rook is good because they cannot move diagonally, so they cannot capture
            // the pawn. So it doesn't matter if we are defended or not.
            if (pawnAttacks & this->enemyKnights_) {
                score += (PieceMaterial::Knight / 2);
            }

            if (pawnAttacks & this->enemyRooks_) {
                score += (PieceMaterial::Rook / 2);
            }

            // Attacking an enemy bishop or queen while we are defended by another pawn is good
            if (this->friendlyPawnAttacks_.get(move.to())) {
                if (pawnAttacks & this->enemyBishops_) {
                    score += (PieceMaterial::Bishop / 2);
                }

                if (pawnAttacks & this->enemyQueens_) {
                    score += (PieceMaterial::Queen / 2);
                }
            }

            // If we are being attacked by an enemy pawn, moving the pawn to get out of danger is probably good
            if (this->enemyPawnAttacks_.get(move.from())) {
                score += (PieceMaterial::Pawn * 2);
            }

            break;
        }

        // Knight heuristics
        case PieceType::Knight: {
            // Use piece-square tables
            constexpr const auto &Table = PieceSquareTables::Knight[Side];
            score += (Table[move.to()] * 10);

            // Moving a knight to a square defended by a friendly pawn is good
            if (this->friendlyPawnAttacks_.get(move.to())) {
                score += 200;
            }

            // Mutual knight defense is good
            if (this->friendlyKnightAttacks_.get(move.to())) {
                score += 100;
            }

            // Moving a knight to a square defended by an enemy bishop or lower is bad
            if (this->enemyBishopOrLowerAttacks_.get(move.to())) {
                score -= (PieceMaterial::Knight * 2);
            }

            // If we are being attacked by an enemy bishop or lower, moving the knight to get out of danger is probably
            // good
            if (this->enemyBishopOrLowerAttacks_.get(move.from())) {
                score += (PieceMaterial::Knight * 2);
            }

            // The knight attacks after this move
            Bitboard knightAttacks = Bitboards::knightAttacks(move.to());

            // Forking enemy rooks, queens, and kings is good
            Bitboard forked = knightAttacks & this->enemyRookOrHigher_;
            if (forked.count() >= 2) {
                score += 300;
            }

            break;
        }

        // Bishop heuristics
        case PieceType::Bishop: {
            // Use piece-square tables
            constexpr const auto &Table = PieceSquareTables::Bishop[Side];
            score += (Table[move.to()] * 10);

            // Moving a bishop to a square defended by a friendly pawn is good because it's a fortress
            if (this->friendlyPawnAttacks_.get(move.to())) {
                score += 100;
            }

            // Moving a bishop to a square defended by an enemy bishop or lower is bad
            if (this->enemyBishopOrLowerAttacks_.get(move.to())) {
                score -= (PieceMaterial::Bishop * 2);
            }

            // If we are being attacked by an enemy bishop or lower, moving the bishop to get out of danger is probably
            // good
            if (this->enemyBishopOrLowerAttacks_.get(move.from())) {
                score += (PieceMaterial::Bishop * 2);
            }

            break;
        }

        // Rook heuristics
        case PieceType::Rook: {
            // Use piece-square tables
            constexpr const auto &Table = PieceSquareTables::Rook[Side];
            score += (Table[move.to()] * 10);

            // Moving a rook to a square defended by an enemy rook or lower is bad
            if (this->enemyRookOrLowerAttacks_.get(move.to())) {
                score -= (PieceMaterial::Rook * 2);
            }

            // If we are being attacked by an enemy rook or lower, moving the rook to get out of danger is probably good
            if (this->enemyRookOrLowerAttacks_.get(move.from())) {
                score += (PieceMaterial::Rook * 2);
            }

            break;
        }

        // Queen heuristics
        case PieceType::Queen: {
            // Use piece-square tables
            constexpr const auto &Table = PieceSquareTables::Queen[Side];
            score += (Table[move.to()] * 10);

            // Moving a queen to a square defended by an enemy queen or lower is bad
            if (this->enemyQueenOrLowerAttacks_.get(move.to())) {
                score -= (PieceMaterial::Queen * 2);
            }

            // If we are being attacked by an enemy queen or lower, moving the queen to get out of danger is probably
            // good
            if (this->enemyQueenOrLowerAttacks_.get(move.from())) {
                score += (PieceMaterial::Queen * 2);
            }

            break;
        }

        default: break;
    }

    return score;
}



template<Color Side, uint32_t Flags>
void MoveOrdering::score(MovePriorityQueue &moves, const Board &board, const HistoryTable *history) {
    MoveScorer<Side, Flags> scorer(board, history);

    for (MoveEntry *entry = moves.start(); entry != moves.end(); ++entry) {
        entry->score = scorer.score(entry->move);
    }
}



template<Color Side, uint32_t Flags>
INLINE void scoreRoot(RootMoveList &moves, const Board &board, const HistoryTable *history) {
    MoveScorer<Side, Flags> scorer(board, history);

    for (MoveEntry &entry : moves.moves()) {
        entry.score = scorer.score(entry.move);
    }
}

template<uint32_t Flags>
void MoveOrdering::score(RootMoveList &moves, const Board &board, const HistoryTable *history) {
    if (board.turn() == Color::White) {
        scoreRoot<Color::White, Flags>(moves, board, history);
    } else {
        scoreRoot<Color::Black, Flags>(moves, board, history);
    }
}

// @formatter:off
template void MoveOrdering::score<Color::White, MoveOrdering::Type::NoHistory>(MovePriorityQueue &, const Board &, const HistoryTable *);
template void MoveOrdering::score<Color::Black, MoveOrdering::Type::NoHistory>(MovePriorityQueue &, const Board &, const HistoryTable *);
template void MoveOrdering::score<Color::White, MoveOrdering::Type::History>(MovePriorityQueue &, const Board &, const HistoryTable *);
template void MoveOrdering::score<Color::Black, MoveOrdering::Type::History>(MovePriorityQueue &, const Board &, const HistoryTable *);

template void MoveOrdering::score<MoveOrdering::Type::NoHistory>(RootMoveList &, const Board &, const HistoryTable *);
template void MoveOrdering::score<MoveOrdering::Type::History>(RootMoveList &, const Board &, const HistoryTable *);
// @formatter:on
