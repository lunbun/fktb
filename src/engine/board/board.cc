#include "board.h"

#include <cstdint>
#include <string>
#include <cassert>

#include "piece.h"
#include "fen.h"
#include "engine/hash/transposition.h"
#include "engine/inline.h"

Board::Board(Color turn, CastlingRights castlingRights) : material_(), turn_(turn), hash_(0),
                                                          castlingRights_(castlingRights), pieces_(), bitboards_() {
    this->pieces_.fill(Piece::empty());
    this->bitboards_.white().fill(0);
    this->bitboards_.black().fill(0);

    this->hash_ ^= Zobrist::castlingRights(castlingRights);
    if (turn == Color::Black) {
        this->hash_ ^= Zobrist::blackToMove();
    }
}

Board::~Board() = default;

Board Board::startingPosition() {
    return Board::fromFen(Board::StartingFen);
}

Board Board::fromFen(const std::string &fen) {
    FenReader reader(fen);
    Board board(reader.turn(), reader.castlingRights());

    while (reader.hasNext()) {
        auto entry = reader.next();

        board.addPiece<true>(entry.piece, entry.square);
    }

    return board;
}

std::string Board::toFen() const {
    FenWriter writer;

    for (int8_t rank = 7; rank >= 0; rank--) {
        for (int8_t file = 0; file < 8; file++) {
            auto piece = this->pieceAt(Square(file, rank));

            if (piece.isEmpty()) {
                writer.empty();
            } else {
                writer.piece(piece);
            }
        }

        writer.nextRank();
    }

    writer.turn(this->turn_);
    writer.castlingRights(this->castlingRights_);

    return writer.fen();
}

Board Board::copy() const {
    return Board::fromFen(this->toFen());
}



// TODO: This function is expensive.
template<Color Side>
bool Board::isInCheck() const {
    constexpr Color Enemy = ~Side;

    Bitboard occupied = this->occupied();
    Bitboard attacks = Bitboards::allPawn<Enemy>(this->bitboard<Enemy>(PieceType::Pawn))
        | Bitboards::allKnight(this->bitboard<Enemy>(PieceType::Knight))
        | Bitboards::allBishop(this->bitboard<Enemy>(PieceType::Bishop), occupied)
        | Bitboards::allRook(this->bitboard<Enemy>(PieceType::Rook), occupied)
        | Bitboards::allQueen(this->bitboard<Enemy>(PieceType::Queen), occupied)
        | Bitboards::allKing(this->bitboard<Enemy>(PieceType::King));
    Bitboard king = this->bitboard<Side>(PieceType::King);

    return (attacks & king);
}

template bool Board::isInCheck<Color::White>() const;
template bool Board::isInCheck<Color::Black>() const;



template<bool UpdateHash>
INLINE void Board::addPiece(Piece piece, Square square) {
    assert(this->pieceAt(square).isEmpty());
    this->pieces_[square] = piece;

    assert(!this->bitboard(piece.type(), piece.color()).get(square));
    this->bitboard(piece.type(), piece.color()).set(square);

    this->material_[piece.color()] += piece.material();

    if constexpr (UpdateHash) {
        this->hash_ ^= Zobrist::piece(piece, square);
    }
}

template<bool UpdateHash>
INLINE void Board::removePiece(Piece piece, Square square) {
    assert(this->pieceAt(square) == piece);
    this->pieces_[square] = Piece::empty();

    assert(this->bitboard(piece.type(), piece.color()).get(square));
    this->bitboard(piece.type(), piece.color()).clear(square);

    this->material_[piece.color()] -= piece.material();

    if constexpr (UpdateHash) {
        this->hash_ ^= Zobrist::piece(piece, square);
    }
}

// Updates the hash and castling rights.
INLINE void Board::castlingRights(CastlingRights newCastlingRights) {
    this->hash_ ^= Zobrist::castlingRights(this->castlingRights_);
    this->hash_ ^= Zobrist::castlingRights(newCastlingRights);

    this->castlingRights_ = newCastlingRights;
}

// Removes the relevant castling rights if the given square is a rook square.
INLINE void Board::maybeRevokeCastlingRightsForRookSquare(Square square) {
    constexpr Bitboard CastlingRookSquares = Bitboards::A1 | Bitboards::H1 | Bitboards::A8 | Bitboards::H8;

    // Make sure the square is indeed a castling rook square
    if (!CastlingRookSquares.get(square)) {
        return;
    }

    CastlingRights revokedRights = CastlingRights::fromRookSquare(square);
    this->castlingRights(this->castlingRights_.without(revokedRights));
}

// Moves a piece from one square to another.
template<bool UpdateHash>
INLINE void Board::movePiece(Piece piece, Square from, Square to) {
    assert(!piece.isEmpty());
    assert(from != to);

    assert(this->pieceAt(from) == piece);
    assert(this->pieceAt(to).isEmpty());
    this->pieces_[from] = Piece::empty();
    this->pieces_[to] = piece;

    Bitboard &bitboard = this->bitboard(piece.type(), piece.color());
    assert(bitboard.get(from));
    assert(!bitboard.get(to));
    bitboard.clear(from);
    bitboard.set(to);

    if constexpr (UpdateHash) {
        this->hash_ ^= Zobrist::piece(piece, from);
        this->hash_ ^= Zobrist::piece(piece, to);
    }
}

// Moves/unmoves a piece from one square to another. Will not update hash if it is an unmake. Returns the moved piece.
template<bool IsMake>
INLINE Piece Board::moveOrUnmovePiece(Square from, Square to) {
    if constexpr (IsMake) {
        Piece piece = this->pieceAt(from);
        this->movePiece<true>(piece, from, to);
        return piece;
    } else {
        Piece piece = this->pieceAt(to);

        // Disabled linting on the next line because it is flagged for "argument selection defects," where it thinks
        // that we erroneously swapped the from and to squares. This is actually correct though since we are unmaking
        // the move.
        this->movePiece<false>(piece, to, from); // NOLINT

        return piece;
    }
}

// Makes/unmakes a castling move. Will not update hash or castling rights if it is an unmake.
template<bool IsMake>
INLINE void Board::makeCastlingMove(Move move) {
    // Move the king
    Piece king = this->moveOrUnmovePiece<IsMake>(move.from(), move.to());

    // Move the rook
    Square rookFrom = CastlingRook::from(king.color(), move.castlingSide());
    Square rookTo = CastlingRook::to(king.color(), move.castlingSide());
    this->moveOrUnmovePiece<IsMake>(rookFrom, rookTo);

    // Update castling rights
    if constexpr (IsMake) {
        this->castlingRights(this->castlingRights_.without(king.color()));
    }
}

// Makes/unmakes a promotion move. Will not update hash if it is an unmake.
template<bool IsMake>
INLINE void Board::makePromotionMove(Move move) {
    if constexpr (IsMake) {
        Piece pawn = this->pieceAt(move.from());

        // Remove the old pawn
        this->removePiece<true>(pawn, move.from());

        // Add the new piece
        Piece promotion = Piece(move.promotion(), pawn.color());
        this->addPiece<true>(promotion, move.to());
    } else {
        Piece promotion = this->pieceAt(move.to());

        // Remove the promoted piece
        this->removePiece<false>(promotion, move.to());

        // Add the pawn
        this->addPiece<false>(Piece::pawn(promotion.color()), move.from());
    }
}

// Makes/unmakes a quiet move. Will not update hash or castling rights if it is an unmake.
template<bool IsMake>
INLINE void Board::makeQuietMove(Move move) {
    // Move/unmove the piece
    Piece piece = this->moveOrUnmovePiece<IsMake>(move.from(), move.to());

    // Update castling rights if necessary
    if constexpr (IsMake) {
        if (piece.type() == PieceType::Rook) {
            // Update castling rights if necessary
            this->maybeRevokeCastlingRightsForRookSquare(move.from());
        } else if (piece.type() == PieceType::King) {
            // Update castling rights
            this->castlingRights(this->castlingRights_.without(piece.color()));
        }
    }
}

template<bool UpdateTurn>
MakeMoveInfo Board::makeMove(Move move) {
    uint64_t oldHash = this->hash_;
    CastlingRights oldCastlingRights = this->castlingRights_;

    Piece captured = Piece::empty();
    if (move.isCapture()) { // Captures
        // Check if the move is a capture before getting the captured piece to avoid unnecessary calls to pieceAt
        // (avoids unnecessary memory reads)
        captured = this->pieceAt(move.to());
        assert(captured.type() != PieceType::King);
        this->removePiece<true>(captured, move.to());

        // If a rook is captured, update castling rights if necessary
        if (captured.type() == PieceType::Rook) {
            this->maybeRevokeCastlingRightsForRookSquare(move.to());
        }
    }

    if (move.isCastle()) { // Castling
        this->makeCastlingMove<true>(move);
    } else if (move.isPromotion()) { // Promotions
        this->makePromotionMove<true>(move);
    } else { // Quiet moves
        this->makeQuietMove<true>(move);
    }

    // Switch the turn
    this->hash_ ^= Zobrist::blackToMove();
    if constexpr (UpdateTurn) {
        this->turn_ = ~this->turn_;
    }

    return { oldHash, oldCastlingRights, captured };
}

template<bool UpdateTurn>
void Board::unmakeMove(Move move, MakeMoveInfo info) {
    // We don't need to do any hash updates or castling rights updates here since we just restore the old hash and
    // castling rights from MakeMoveInfo

    if (move.isCastle()) { // Castling
        this->makeCastlingMove<false>(move);
    } else if (move.isPromotion()) { // Promotions
        this->makePromotionMove<false>(move);
    } else { // Normal moves
        this->makeQuietMove<false>(move);
    }

    if (move.isCapture()) { // Captures
        // Add the captured piece back
        this->addPiece<false>(info.captured, move.to());
    }

    // Restore the old hash and castling rights
    this->hash_ = info.oldHash;
    this->castlingRights_ = info.oldCastlingRights;

    // Switch the turn if necessary
    if constexpr (UpdateTurn) {
        this->turn_ = ~this->turn_;
    }
}

template MakeMoveInfo Board::makeMove<false>(Move move);
template MakeMoveInfo Board::makeMove<true>(Move move);
template void Board::unmakeMove<false>(Move move, MakeMoveInfo info);
template void Board::unmakeMove<true>(Move move, MakeMoveInfo info);
