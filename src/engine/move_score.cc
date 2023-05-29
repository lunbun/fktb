#include "move_score.h"

#include "piece.h"
#include "bitboard.h"
#include "board.h"
#include "move.h"
#include "evaluation.h"

template<Color Side>
MoveScorer<Side>::MoveScorer(const Board &board) : board_(board) {
    constexpr Color Enemy = ~Side;

    Bitboard occupied = board.occupied();

    this->friendlyPawnAttacks_ = Bitboards::allPawn<Side>(board.bitboard<Side>(PieceType::Pawn));
    this->friendlyKnightAttacks_ = Bitboards::allKnight(board.bitboard<Side>(PieceType::Knight));

    this->enemyKnights_ = board.bitboard<Enemy>(PieceType::Knight);
    this->enemyBishops_ = board.bitboard<Enemy>(PieceType::Bishop);
    this->enemyRooks_ = board.bitboard<Enemy>(PieceType::Rook);
    this->enemyQueens_ = board.bitboard<Enemy>(PieceType::Queen);
    Bitboard enemyKings = board.bitboard<Enemy>(PieceType::King);

    this->enemyPawnAttacks_ = Bitboards::allPawn<Enemy>(board.bitboard<Enemy>(PieceType::Pawn));
    this->enemyKnightAttacks_ = Bitboards::allKnight(this->enemyKnights_);
    this->enemyBishopAttacks_ = Bitboards::allBishop(this->enemyBishops_, occupied);
    Bitboard enemyRookAttacks = Bitboards::allRook(this->enemyRooks_, occupied);
    Bitboard enemyQueenAttacks = Bitboards::allQueen(this->enemyQueens_, occupied);

    this->enemyRookOrHigher_ = this->enemyRooks_ | this->enemyQueens_ | enemyKings;
    this->enemyBishopOrLowerAttacks_ = this->enemyBishopAttacks_ | this->enemyKnightAttacks_ | this->enemyPawnAttacks_;
    this->enemyRookOrLowerAttacks_ = enemyRookAttacks | this->enemyBishopOrLowerAttacks_;
    this->enemyQueenOrLowerAttacks_ = enemyQueenAttacks | this->enemyRookOrLowerAttacks_;
}

template<Color Side>
int32_t MoveScorer<Side>::score(Move move) {
    int32_t score = 0;

    const Board &board = this->board_;

    Piece piece = board.pieceAt(move.from());


    // Captures
    if (move.isCapture()) {
        Piece captured = board.pieceAt(move.to());
        score += (captured.material() * 10 - piece.material());
    }

    // Promotions
    if (move.isPromotion()) {
        score += (PieceMaterial::material(move.promotion()) * 10);
    }

    switch (piece.type()) {
        // Pawn heuristics
        case PieceType::Pawn: {
            // Use piece-square tables
            constexpr const PieceSquareTable &Table = Evaluation::pawnTable[Side];
            score += (Table[move.to()] * 5);

            // The pawn attacks after this move
            Bitboard pawnAttacks = Bitboards::pawn<Side>(move.to());

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

            break;
        }

        // Knight heuristics
        case PieceType::Knight: {
            // Use piece-square tables
            constexpr const PieceSquareTable &Table = Evaluation::knightTable[Side];
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

            // The knight attacks after this move
            Bitboard knightAttacks = Bitboards::knight(move.to());

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
            constexpr const PieceSquareTable &Table = Evaluation::bishopTable[Side];
            score += (Table[move.to()] * 10);

            // Moving a bishop to a square defended by a friendly pawn is good because it's a fortress
            if (this->friendlyPawnAttacks_.get(move.to())) {
                score += 100;
            }

            // Moving a bishop to a square defended by an enemy bishop or lower is bad
            if (this->enemyBishopOrLowerAttacks_.get(move.to())) {
                score -= (PieceMaterial::Bishop * 2);
            }

            break;
        }

        // Rook heuristics
        case PieceType::Rook: {
            // Use piece-square tables
            constexpr const PieceSquareTable &Table = Evaluation::rookTable[Side];
            score += (Table[move.to()] * 10);

            // Moving a rook to a square defended by an enemy rook or lower is bad
            if (this->enemyRookOrLowerAttacks_.get(move.to())) {
                score -= (PieceMaterial::Rook * 2);
            }

            break;
        }

        // Queen heuristics
        case PieceType::Queen: {
            // Use piece-square tables
            constexpr const PieceSquareTable &Table = Evaluation::queenTable[Side];
            score += (Table[move.to()] * 10);

            // Moving a queen to a square defended by an enemy queen or lower is bad
            if (this->enemyQueenOrLowerAttacks_.get(move.to())) {
                score -= (PieceMaterial::Queen * 2);
            }

            break;
        }

        default: break;
    }

    return score;
}



template class MoveScorer<Color::White>;
template class MoveScorer<Color::Black>;
