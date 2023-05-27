#include "move_score.h"

#include "piece.h"
#include "bitboard.h"
#include "board.h"
#include "move.h"

template<Color Side>
MoveScorer<Side>::MoveScorer(const Board &board) : board_(board) {
    constexpr Color Enemy = ~Side;

    this->friendlyPawnAttacks_ = Bitboards::allPawn<Side>(board.bitboard<Side>(PieceType::Pawn));
    this->friendlyKnightAttacks_ = Bitboards::allKnight(board.bitboard<Side>(PieceType::Knight));

    this->enemyKnights_ = board.bitboard<Enemy>(PieceType::Knight);
    this->enemyRooks_ = board.bitboard<Enemy>(PieceType::Rook);
    this->enemyPawnAttacks_ = Bitboards::allPawn<Enemy>(board.bitboard<Enemy>(PieceType::Pawn));
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
            if (!move.isCapture()) {
                // The pawn attacks after this pawn push
                Bitboard pawnAttacks = Bitboards::pawn<Side>(move.to());

                // Attacking an enemy knight or rook is good because they cannot move diagonally, so they cannot capture
                // the pawn. So it doesn't matter if we are defended or not.
                if (pawnAttacks & this->enemyKnights_) {
                    score += 100;
                }

                if (pawnAttacks & this->enemyRooks_) {
                    score += 200;
                }
            }

            break;
        }

        // Knight heuristics
        case PieceType::Knight: {
            // Moving a knight to a square defended by a friendly pawn is good
            if (this->friendlyPawnAttacks_.get(move.to())) {
                score += 200;
            }

            // Mutual knight defense is good
            if (this->friendlyKnightAttacks_.get(move.to())) {
                score += 100;
            }

            break;
        }

        default: break;
    }

    // Moving a piece to a square attacked by an enemy pawn is bad
    if (piece.material() > PieceMaterial::Pawn && this->enemyPawnAttacks_.get(move.to())) {
        score -= (piece.material() - PieceMaterial::Pawn) * 2;
    }

    return score;
}



template class MoveScorer<Color::White>;
template class MoveScorer<Color::Black>;
