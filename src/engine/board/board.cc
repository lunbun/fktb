#include "board.h"

#include <cstdint>
#include <string>
#include <cassert>

#include "piece.h"
#include "fen.h"
#include "engine/inline.h"
#include "engine/hash/transposition.h"
#include "engine/eval/piece_square_table.h"

Board::Board(Color turn, CastlingRights castlingRights, Square enPassantSquare) : material_(), pieceSquareEval_(), turn_(turn),
                                                                                  hash_(0), castlingRights_(castlingRights),
                                                                                  enPassantSquare_(enPassantSquare), pieces_(),
                                                                                  bitboards_(),
                                                                                  kings_(Square::Invalid, Square::Invalid) {
    this->pieces_.fill(Piece::empty());
    this->bitboards_.white().fill(0);
    this->bitboards_.black().fill(0);

    if (turn == Color::Black) {
        this->hash_ ^= Zobrist::blackToMove();
    }
    this->hash_ ^= Zobrist::castlingRights(castlingRights);
    this->hash_ ^= Zobrist::enPassantSquare(enPassantSquare);
}

Board::~Board() = default;

Board Board::startingPosition() {
    return Board::fromFen(Board::StartingFen);
}

Board Board::fromFen(const std::string &fen) {
    FenReader reader(fen);
    Board board(reader.turn(), reader.castlingRights(), reader.enPassantSquare());

    while (reader.hasNext()) {
        auto entry = reader.next();

        Piece piece = entry.piece;
        if (piece.type() == PieceType::King) {
            board.addKing<true>(piece.color(), entry.square);
        } else {
            board.addPiece<true>(piece, entry.square);
        }
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
    writer.enPassantSquare(this->enPassantSquare_);

    return writer.fen();
}

Board Board::copy() const {
    return Board::fromFen(this->toFen());
}



// TODO: This function is expensive.
template<Color Side>
bool Board::isInCheck() const {
    Bitboard attacks = Bitboards::allAttacks<~Side>(*this, this->occupied());
    Square king = this->king(Side);

    return attacks.get(king);
}

template bool Board::isInCheck<Color::White>() const;
template bool Board::isInCheck<Color::Black>() const;



template<bool UpdateHash>
INLINE void Board::addKing(Color color, Square square) {
    assert(this->pieceAt(square).isEmpty());
    this->pieces_[square] = Piece::king(color);

    assert(!this->kings_[color].isValid());
    this->kings_[color] = square;

    if constexpr (UpdateHash) {
        this->hash_ ^= Zobrist::piece(Piece::king(color), square);
    }
}

template<bool UpdateHash>
INLINE void Board::addPiece(Piece piece, Square square) {
    assert(this->pieceAt(square).isEmpty());
    this->pieces_[square] = piece;

    assert(!this->bitboard(piece).get(square));
    this->bitboard(piece).set(square);

    this->material_[piece.color()] += piece.material();
    this->pieceSquareEval_[piece.color()] += PieceSquareTables::evaluate(piece, square);

    if constexpr (UpdateHash) {
        this->hash_ ^= Zobrist::piece(piece, square);
    }
}

template<bool UpdateHash>
INLINE void Board::removePiece(Piece piece, Square square) {
    assert(this->pieceAt(square) == piece);
    this->pieces_[square] = Piece::empty();

    assert(this->bitboard(piece).get(square));
    this->bitboard(piece).clear(square);

    this->material_[piece.color()] -= piece.material();
    this->pieceSquareEval_[piece.color()] -= PieceSquareTables::evaluate(piece, square);

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

// Updates the hash and en passant square.
INLINE void Board::enPassantSquare(Square newEnPassantSquare) {
    this->hash_ ^= Zobrist::enPassantSquare(this->enPassantSquare_);
    this->hash_ ^= Zobrist::enPassantSquare(newEnPassantSquare);

    this->enPassantSquare_ = newEnPassantSquare;
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

    if (piece.type() == PieceType::King) { // King move
        assert(this->kings_[piece.color()] == from);
        this->kings_[piece.color()] = to;
    } else { // Other piece move
        Bitboard &bitboard = this->bitboard(piece);
        assert(bitboard.get(from));
        assert(!bitboard.get(to));
        bitboard.clear(from);
        bitboard.set(to);
    }

    // Update piece square evaluation
    this->pieceSquareEval_[piece.color()] -= PieceSquareTables::evaluate(piece, from);
    this->pieceSquareEval_[piece.color()] += PieceSquareTables::evaluate(piece, to);

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
        Piece promotion(pawn.color(), move.promotion());
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

    // Update castling rights or en passant square if necessary
    if constexpr (IsMake) {
        switch (piece.type()) {
            case PieceType::Pawn: {
                // Update en passant square if double pawn push
                if (move.isDoublePawnPush()) {
                    // TODO: There's probably a better way of getting the en passant square, but for now, just average the from
                    //  and to squares
                    Square enPassantSquare(move.to().file(), (move.to().rank() + move.from().rank()) / 2);
                    this->enPassantSquare(enPassantSquare);
                }
                break;
            }

            case PieceType::Rook: {
                // Update castling rights if necessary
                this->maybeRevokeCastlingRightsForRookSquare(move.from());
                break;
            }

            case PieceType::King: {
                // Update castling rights
                this->castlingRights(this->castlingRights_.without(piece.color()));
                break;
            }

            default: break;
        }
    }
}

template<bool UpdateTurn>
MakeMoveInfo Board::makeMove(Move move) {
    uint64_t oldHash = this->hash_;
    CastlingRights oldCastlingRights = this->castlingRights_;
    Square oldEnPassantSquare = this->enPassantSquare_;

    // Reset en passant square (if the move is a double pawn push, will be set to correct value later during makeQuietMove)
    this->enPassantSquare(Square::Invalid);

    // Captures
    Piece captured = Piece::empty();
    if (move.isCapture()) {
        Square capturedSquare = move.capturedSquare();

        // Check if the move is a capture before getting the captured piece to avoid unnecessary calls to pieceAt
        // (avoids unnecessary memory reads)
        captured = this->pieceAt(capturedSquare);
        assert(captured.type() != PieceType::King);
        this->removePiece<true>(captured, capturedSquare);

        // If a rook is captured, update castling rights if necessary
        if (captured.type() == PieceType::Rook) {
            this->maybeRevokeCastlingRightsForRookSquare(capturedSquare);
        }
    }

    // Make the move
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

    return { oldHash, oldCastlingRights, oldEnPassantSquare, captured };
}

template<bool UpdateTurn>
void Board::unmakeMove(Move move, MakeMoveInfo info) {
    // We don't need to do any hash, castling rights, or en passant square updates here since we just restore the old hash and
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
        this->addPiece<false>(info.captured, move.capturedSquare());
    }

    // Restore the old hash, castling rights, and en passant square
    this->hash_ = info.oldHash;
    this->castlingRights_ = info.oldCastlingRights;
    this->enPassantSquare_ = info.oldEnPassantSquare;

    // Switch the turn if necessary
    if constexpr (UpdateTurn) {
        this->turn_ = ~this->turn_;
    }
}

template MakeMoveInfo Board::makeMove<false>(Move);
template MakeMoveInfo Board::makeMove<true>(Move);
template void Board::unmakeMove<false>(Move, MakeMoveInfo);
template void Board::unmakeMove<true>(Move, MakeMoveInfo);



// Makes/unmakes a null move WITHOUT updating turn_ (updates the hash's turn, but not the turn_ field itself).
MakeMoveInfo Board::makeNullMove() {
    uint64_t oldHash = this->hash_;
    CastlingRights oldCastlingRights = this->castlingRights_;
    Square oldEnPassantSquare = this->enPassantSquare_;

    // Reset en passant square
    this->enPassantSquare(Square::Invalid);

    // Switch the turn
    this->hash_ ^= Zobrist::blackToMove();

    return { oldHash, oldCastlingRights, oldEnPassantSquare, Piece::empty() };
}

void Board::unmakeNullMove(MakeMoveInfo info) {
    // Restore the old hash, castling rights, and en passant square
    this->hash_ = info.oldHash;
    this->castlingRights_ = info.oldCastlingRights;
    this->enPassantSquare_ = info.oldEnPassantSquare;
}
