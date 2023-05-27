#include "move_score.h"

#include "piece.h"
#include "bitboard.h"
#include "board.h"
#include "move.h"

template<Color Side>
MoveScorer<Side>::MoveScorer(const Board &board) : board_(board) {
    constexpr Color Enemy = ~Side;

    this->friendlyPawnAttacks_ = Bitboards::allPawn<Side>(board.bitboard<Side>(PieceType::Pawn));
    this->enemyPawnAttacks_ = Bitboards::allPawn<Enemy>(board.bitboard<Enemy>(PieceType::Pawn));
}

template<Color Side>
int32_t MoveScorer<Side>::score(Move move) {
    int32_t score = 0;

    const Board &board = this->board_;

    Piece piece = board.pieceAt(move.from());
    Piece captured = board.pieceAt(move.to());

    // Captures
    if (!captured.isEmpty()) {
        score += (captured.material() * 10 - piece.material());
    }

    // Moving a piece to a square defended by a friendly pawn is good
    if (this->friendlyPawnAttacks_.get(move.to())) {
        score += std::max(piece.material() - PieceMaterial::Pawn, 0);
    }

    // Moving a piece to a square attacked by an enemy pawn is bad
    if (this->enemyPawnAttacks_.get(move.to())) {
        score -= std::max(piece.material() - PieceMaterial::Pawn, 0);
    }

    return score;
}



template class MoveScorer<Color::White>;
template class MoveScorer<Color::Black>;
