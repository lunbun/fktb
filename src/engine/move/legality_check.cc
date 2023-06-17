#include "legality_check.h"

#include "engine/inline.h"

template<Color Side>
LegalityChecker<Side>::LegalityChecker(Board &board) : board_(board) {
    Bitboard friendly = board.composite(Side);
    this->enemy_ = board.composite(~Side);
    this->occupied_ = friendly | this->enemy_;
    this->empty_ = ~this->occupied_;
}



template<Color Side>
INLINE bool LegalityChecker<Side>::isPseudoLegalQuiet(Move move, Piece piece) const {
    Square from = move.from();
    Square to = move.to();

    if (piece.type() == PieceType::Pawn) {
        // Calling pieceAttacks on a pawn will return the pawn captures, not the pawn pushes. However, we need to check the pawn
        // pushes for quiet moves, so pawns are handled separately.

        // Ensure the pawn is moving forward, and that we are not moving to the promotion rank.
        if constexpr (Side == Color::White) {
            constexpr uint8_t PromotionRank = 7;
            if (to.rank() != from.rank() + 1 || to.rank() == PromotionRank) {
                return false;
            }
        } else {
            constexpr uint8_t PromotionRank = 0;
            if (to.rank() != from.rank() - 1 || to.rank() == PromotionRank) {
                return false;
            }
        }

        // Ensure the pawn is moving straight.
        return (to.file() == from.file())

            // Ensure there is no piece on the to square.
            && this->empty_.get(to);
    } else {
        Bitboard attacks = Bitboards::nonPawnAttacks(piece.type(), from, this->occupied_);

        // Ensure that the piece can move to the square, and that there is no piece on that square.
        return attacks.get(to) && this->empty_.get(to);
    }
}

template<Color Side>
INLINE bool LegalityChecker<Side>::isPseudoLegalDoublePawnPush(Move move, Piece piece) const {
    constexpr uint8_t DoublePushFromRank = (Side == Color::White) ? 1 : 6;
    constexpr uint8_t DoublePushBetweenRank = (Side == Color::White) ? 2 : 5;
    constexpr uint8_t DoublePushToRank = (Side == Color::White) ? 3 : 4;

    Square from = move.from();
    Square between(from.file(), DoublePushBetweenRank);
    Square to = move.to();

    // Ensure the piece is a pawn.
    return (piece.type() == PieceType::Pawn)

        // Ensure the from and to squares are correct.
        && (from.rank() == DoublePushFromRank) && (to.rank() == DoublePushToRank) && (from.file() == to.file())

        // Ensure there are no pieces in the way of the pawn.
        && this->empty_.get(between) && this->empty_.get(to);
}

template<Color Side>
INLINE bool LegalityChecker<Side>::isPseudoLegalKingCastle(Move move, Piece piece) const {
    // Ensure the piece is a king of the correct color.
    if (piece.type() != PieceType::King || piece.color() != Side) {
        return false;
    }

    if constexpr (Side == Color::White) {
        constexpr Bitboard EmptySquares = Bitboards::F1 | Bitboards::G1;

        // Ensure valid castling rights.
        return (this->board_.castlingRights() & CastlingRights::WhiteKingSide)

            // Ensure the from and to squares are correct.
            && (move.from() == Square::E1) && (move.to() == Square::G1)

            // Ensure the king and rook can see each other.
            && (!(this->occupied_ & EmptySquares));
    } else {
        constexpr Bitboard EmptySquares = Bitboards::F8 | Bitboards::G8;

        // Ensure valid castling rights.
        return (this->board_.castlingRights() & CastlingRights::BlackKingSide)

            // Ensure the from and to squares are correct.
            && (move.from() == Square::E8) && (move.to() == Square::G8)

            // Ensure the king and rook can see each other.
            && (!(this->occupied_ & EmptySquares));
    }
}

template<Color Side>
INLINE bool LegalityChecker<Side>::isPseudoLegalQueenCastle(Move move, Piece piece) const {
    // Ensure the piece is a king of the correct color.
    if (piece.type() != PieceType::King || piece.color() != Side) {
        return false;
    }

    if constexpr (Side == Color::White) {
        constexpr Bitboard EmptySquares = Bitboards::B1 | Bitboards::C1 | Bitboards::D1;

        // Ensure valid castling rights.
        return (this->board_.castlingRights() & CastlingRights::WhiteQueenSide)

            // Ensure the from and to squares are correct.
            && (move.from() == Square::E1) && (move.to() == Square::C1)

            // Ensure the king and rook can see each other.
            && (!(this->occupied_ & EmptySquares));
    } else {
        constexpr Bitboard EmptySquares = Bitboards::B8 | Bitboards::C8 | Bitboards::D8;

        // Ensure valid castling rights.
        return (this->board_.castlingRights() & CastlingRights::BlackQueenSide)

            // Ensure the from and to squares are correct.
            && (move.from() == Square::E8) && (move.to() == Square::C8)

            // Ensure the king and rook can see each other.
            && (!(this->occupied_ & EmptySquares));
    }
}

template<Color Side>
INLINE bool LegalityChecker<Side>::isPseudoLegalCapture(Move move, Piece piece, Piece captured) const {
    Bitboard attacks = Bitboards::pieceAttacks<Side>(piece.type(), move.from(), this->occupied_);

    // Ensure that the piece can move to the square, and that the captured piece is an enemy piece.
    return attacks.get(move.to()) && (!captured.isEmpty()) && (captured.color() == ~Side);
}

template<Color Side>
INLINE bool LegalityChecker<Side>::isPseudoLegalEnPassant(Move move, Piece piece, Piece captured) const {
    constexpr uint8_t EnPassantFromRank = (Side == Color::White) ? 4 : 3;
    constexpr uint8_t EnPassantToRank = (Side == Color::White) ? 5 : 2;

    Square from = move.from();
    Square to = move.to();

    // Ensure both the piece and captured piece are pawns.
    return (piece.type() == PieceType::Pawn) && (captured.type() == PieceType::Pawn) && (captured.color() == ~Side)

        // Ensure the to square is the en passant square.
        && (to == this->board_.enPassantSquare())

        // Ensure the from and to ranks are correct.
        && (from.rank() == EnPassantFromRank) && (to.rank() == EnPassantToRank)

        // Ensure the from and to files are one file apart.
        && (from.file() == to.file() - 1 || from.file() == to.file() + 1);
}

template<Color Side>
template<bool IsCapture>
INLINE bool LegalityChecker<Side>::isPseudoLegalPromotion(Move move, Piece piece, Piece captured) const {
    constexpr uint8_t PromotionFromRank = (Side == Color::White) ? 6 : 1;
    constexpr uint8_t PromotionToRank = (Side == Color::White) ? 7 : 0;

    Square from = move.from();
    Square to = move.to();

    if constexpr (IsCapture) {
        // Ensure the captured piece is an enemy piece.
        if (captured.isEmpty() || captured.color() == Side) {
            return false;
        }
    } else {
        // Ensure no piece is on the to square.
        if (!this->empty_.get(to)) {
            return false;
        }
    }

    // Ensure the piece is a pawn.
    return (piece.type() == PieceType::Pawn)

        // Ensure the from and to ranks are correct.
        && (from.rank() == PromotionFromRank) && (to.rank() == PromotionToRank)

        // Ensure the from and to files are the same.
        && (from.file() == to.file());
}

// Checks if a move is pseudo-legal.
template<Color Side>
bool LegalityChecker<Side>::isPseudoLegal(Move move) const {
    if (!move.isValid()) {
        return false;
    }

    Piece piece = this->board_.pieceAt(move.from());

    if (piece.isEmpty() || piece.color() != Side) {
        return false;
    }

    Piece captured = this->board_.pieceAt(move.capturedSquare());

    switch (move.flags()) {
        case MoveFlag::Quiet: return this->isPseudoLegalQuiet(move, piece);
        case MoveFlag::DoublePawnPush: return this->isPseudoLegalDoublePawnPush(move, piece);
        case MoveFlag::KingCastle: return this->isPseudoLegalKingCastle(move, piece);
        case MoveFlag::QueenCastle: return this->isPseudoLegalQueenCastle(move, piece);
        case MoveFlag::Capture: return this->isPseudoLegalCapture(move, piece, captured);
        case MoveFlag::EnPassant: return this->isPseudoLegalEnPassant(move, piece, captured);

        case MoveFlag::KnightPromotion:
        case MoveFlag::BishopPromotion:
        case MoveFlag::RookPromotion:
        case MoveFlag::QueenPromotion: return this->isPseudoLegalPromotion<false>(move, piece, captured);

        case MoveFlag::KnightPromoCapture:
        case MoveFlag::BishopPromoCapture:
        case MoveFlag::RookPromoCapture:
        case MoveFlag::QueenPromoCapture: return this->isPseudoLegalPromotion<true>(move, piece, captured);

        default: return false;
    }
}



template<Color Side>
bool LegalityChecker<Side>::isLegalCastle(Move move) const {
    // For castling moves, we need to ensure that the king is not in check, and does not castle through check.
    Bitboard enemyAttacks = Bitboards::allAttacks<~Side>(this->board_, this->occupied_);

    if constexpr (Side == Color::White) {
        // Squares that must not be attacked for castling to be legal.
        constexpr Bitboard KingSideCheck = Bitboards::E1 | Bitboards::F1 | Bitboards::G1;
        constexpr Bitboard QueenSideCheck = Bitboards::C1 | Bitboards::D1 | Bitboards::E1;

        Bitboard check = (move.castlingSide() == CastlingSide::King) ? KingSideCheck : QueenSideCheck;
        return !(enemyAttacks & check);
    } else if constexpr (Side == Color::Black) {
        // Squares that must not be attacked for castling to be legal.
        constexpr Bitboard KingSideCheck = Bitboards::E8 | Bitboards::F8 | Bitboards::G8;
        constexpr Bitboard QueenSideCheck = Bitboards::C8 | Bitboards::D8 | Bitboards::E8;

        Bitboard check = (move.castlingSide() == CastlingSide::King) ? KingSideCheck : QueenSideCheck;
        return !(enemyAttacks & check);
    }
}

// Checks if a move is both pseudo-legal and legal.
template<Color Side>
bool LegalityChecker<Side>::isLegal(Move move) const {
    if (!this->isPseudoLegal(move)) {
        return false;
    }

    Board &board = this->board_;

    if (move.isCastle()) {
        return this->isLegalCastle(move);
    } else {
        // Board::isInCheck() only uses bitboards, so we can use MakeMoveType::BitboardsOnly.
        MakeMoveInfo info = board.makeMove<MakeMoveType::BitboardsOnly>(move);

        bool isLegal = !board.isInCheck<Side>();

        board.unmakeMove<MakeMoveType::BitboardsOnly>(move, info);

        return isLegal;
    }
}



template class LegalityChecker<Color::White>;
template class LegalityChecker<Color::Black>;



// Checks if a move is both pseudo-legal and legal.
// Warning: This is slow because it constructs a LegalityChecker every call, and uses a branch to determine the side. In
// performance-critical code, use the LegalityChecker template directly.
bool LegalityCheck::isLegal(Board &board, Move move) {
    if (board.turn() == Color::White) {
        return LegalityChecker<Color::White>(board).isLegal(move);
    } else {
        return LegalityChecker<Color::Black>(board).isLegal(move);
    }
}
