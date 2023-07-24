#include "board.h"

#include <cstdint>
#include <string>
#include <cassert>

#include "piece.h"
#include "fen.h"
#include "engine/inline.h"
#include "engine/hash/transposition.h"
#include "engine/eval/piece_square_table.h"

namespace FKTB {

Board::Board(Color turn, CastlingRights castlingRights, Square enPassantSquare) : material_(), pieceSquareEval_(), turn_(turn),
                                                                                  hash_(0), castlingRights_(castlingRights),
                                                                                  enPassantSquare_(enPassantSquare), pieces_(),
                                                                                  bitboards_(), repetitionHashes_(),
                                                                                  pliesSinceIrreversible_(0),
                                                                                  kings_(Square::Invalid, Square::Invalid) {
    this->pieces_.fill(Piece::empty());
    this->bitboards_.white().fill(0);
    this->bitboards_.black().fill(0);

    if (turn == Color::Black) {
        this->hash_ ^= Zobrist::blackToMove();
    }
    this->hash_ ^= Zobrist::castlingRights(castlingRights);
    this->hash_ ^= Zobrist::enPassantSquare(enPassantSquare);

    this->repetitionHashes_.reserve(64);
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
            board.addKing<MakeMoveType::All>(piece.color(), entry.square);
        } else {
            board.addPiece<MakeMoveType::All>(piece, entry.square);
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

// Returns true if this position has existed before in the game.
bool Board::isTwofoldRepetition() const {
    // In order for a repetition to occur, there must be at least 4 plies since the last irreversible move because it takes at
    // minimum 4 plies for both sides to shuffle back-and-forth to repeat a position.
    constexpr uint32_t MinPliesSinceIrreversible = 4;

    if (this->repetitionHashes_.empty() || this->pliesSinceIrreversible_ < MinPliesSinceIrreversible) {
        return false;
    }

    // Iterate backwards through the repetition hashes, starting at the most recent one.
    // We only have to check every other hash because of the turn.
    //
    // Kinda a hack, but using signed integers to avoid unsigned index underflow :)
    auto startIndex = static_cast<int32_t>(this->repetitionHashes_.size() - MinPliesSinceIrreversible);
    auto endIndex = static_cast<int32_t>(this->repetitionHashes_.size() - this->pliesSinceIrreversible_);
    for (int32_t i = startIndex; i >= endIndex; i -= 2) {
        if (this->repetitionHashes_[i] == this->hash_) {
            return true;
        }
    }

    return false;
}



template<uint32_t Flags>
INLINE void Board::addKing(Color color, Square square) {
    Piece king = Piece::king(color);

    assert(!this->kings_[color].isValid());
    this->kings_[color] = square;

    assert(this->pieceAt(square).isEmpty());
    this->pieces_[square] = king;

    if constexpr (Flags & MakeMoveFlags::Evaluation) {
        this->pieceSquareEval_[GamePhase::Opening][color] += PieceSquareTables::evaluate(GamePhase::Opening, king, square);
        this->pieceSquareEval_[GamePhase::End][color] += PieceSquareTables::evaluate(GamePhase::End, king, square);
    }

    if constexpr (Flags & MakeMoveFlags::Hash) {
        this->hash_ ^= Zobrist::piece(Piece::king(color), square);
    }
}

template<uint32_t Flags>
INLINE void Board::addPiece(Piece piece, Square square) {
    assert(this->pieceAt(square).isEmpty());
    this->pieces_[square] = piece;

    if constexpr (Flags & MakeMoveFlags::Bitboards) {
        assert(!this->bitboard(piece).get(square));
        this->bitboard(piece).set(square);
    }

    if constexpr (Flags & MakeMoveFlags::Evaluation) {
        this->material_[piece.color()] += piece.material();
        this->pieceSquareEval_[GamePhase::Opening][piece.color()] += PieceSquareTables::evaluate(GamePhase::Opening, piece,
            square);
        this->pieceSquareEval_[GamePhase::End][piece.color()] += PieceSquareTables::evaluate(GamePhase::End, piece, square);
    }

    if constexpr (Flags & MakeMoveFlags::Hash) {
        this->hash_ ^= Zobrist::piece(piece, square);
    }
}

template<uint32_t Flags>
INLINE void Board::removePiece(Piece piece, Square square) {
    assert(this->pieceAt(square) == piece);
    this->pieces_[square] = Piece::empty();

    if constexpr (Flags & MakeMoveFlags::Bitboards) {
        assert(this->bitboard(piece).get(square));
        this->bitboard(piece).clear(square);
    }

    if constexpr (Flags & MakeMoveFlags::Evaluation) {
        this->material_[piece.color()] -= piece.material();
        this->pieceSquareEval_[GamePhase::Opening][piece.color()] -= PieceSquareTables::evaluate(GamePhase::Opening, piece,
            square);
        this->pieceSquareEval_[GamePhase::End][piece.color()] -= PieceSquareTables::evaluate(GamePhase::End, piece, square);
    }

    if constexpr (Flags & MakeMoveFlags::Hash) {
        this->hash_ ^= Zobrist::piece(piece, square);
    }
}

// Updates the hash and castling rights.
template<uint32_t Flags>
INLINE void Board::castlingRights(CastlingRights newCastlingRights) {
    if constexpr (Flags & MakeMoveFlags::Hash) {
        this->hash_ ^= Zobrist::castlingRights(this->castlingRights_);
        this->hash_ ^= Zobrist::castlingRights(newCastlingRights);
    }

    if constexpr (Flags & MakeMoveFlags::Gameplay) {
        this->castlingRights_ = newCastlingRights;
    }
}

// Removes the relevant castling rights if the given square is a rook square.
template<uint32_t Flags>
INLINE void Board::maybeRevokeCastlingRightsForRookSquare(Square square) {
    constexpr Bitboard CastlingRookSquares = Bitboards::A1 | Bitboards::H1 | Bitboards::A8 | Bitboards::H8;

    // Make sure the square is indeed a castling rook square
    if (!CastlingRookSquares.get(square)) {
        return;
    }

    CastlingRights revokedRights = CastlingRights::fromRookSquare(square);
    this->castlingRights<Flags>(this->castlingRights_.without(revokedRights));
}

// Updates the hash and en passant square.
template<uint32_t Flags>
INLINE void Board::enPassantSquare(Square newEnPassantSquare) {
    if constexpr (Flags & MakeMoveFlags::Hash) {
        this->hash_ ^= Zobrist::enPassantSquare(this->enPassantSquare_);
        this->hash_ ^= Zobrist::enPassantSquare(newEnPassantSquare);
    }

    if constexpr (Flags & MakeMoveFlags::Gameplay) {
        this->enPassantSquare_ = newEnPassantSquare;
    }
}

// Updates the repetition hashes for making a move.
template<uint32_t Flags>
INLINE void Board::updateRepetitionHashes(Move move) {
    static_assert(!(Flags & MakeMoveFlags::Unmake), "Can only update repetition hashes for making a move");
    static_assert(Flags & MakeMoveFlags::Repetition, "Can only update repetition hashes if repetition is enabled");

    // Push the current hash onto the repetition list
    this->repetitionHashes_.push_back(this->hash_);

    // Check if the move is irreversible
    // TODO: Moves that lose castling rights are also irreversible
    //  (imperfect move irreversibility does not affect the results, it just makes it slower because it has to check more previous
    //  positions)
    bool isIrreversible = move.isCapture() || move.isPromotion() || move.isCastle()
        || (this->pieceAt(move.from()).type() == PieceType::Pawn);

    if (isIrreversible) {
        this->pliesSinceIrreversible_ = 0;
    } else {
        this->pliesSinceIrreversible_++;
    }
}

// Moves/unmoves a piece from one square to another. Returns the piece that was moved.
template<uint32_t Flags>
INLINE Piece Board::movePiece(Square from, Square to) {
    if constexpr (Flags & MakeMoveFlags::Unmake) {
        // Swap the from and to squares if we're unmoving the piece
        std::swap(from, to);
    }

    Piece piece = this->pieceAt(from);

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
        if constexpr (Flags & MakeMoveFlags::Bitboards) {
            Bitboard &bitboard = this->bitboard(piece);
            assert(bitboard.get(from));
            assert(!bitboard.get(to));
            bitboard.clear(from);
            bitboard.set(to);
        }
    }

    // Update piece square evaluation
    if constexpr (Flags & MakeMoveFlags::Evaluation) {
        this->pieceSquareEval_[GamePhase::Opening][piece.color()] -= PieceSquareTables::evaluate(GamePhase::Opening, piece, from);
        this->pieceSquareEval_[GamePhase::Opening][piece.color()] += PieceSquareTables::evaluate(GamePhase::Opening, piece, to);
        this->pieceSquareEval_[GamePhase::End][piece.color()] -= PieceSquareTables::evaluate(GamePhase::End, piece, from);
        this->pieceSquareEval_[GamePhase::End][piece.color()] += PieceSquareTables::evaluate(GamePhase::End, piece, to);
    }

    if constexpr (Flags & MakeMoveFlags::Hash) {
        this->hash_ ^= Zobrist::piece(piece, from);
        this->hash_ ^= Zobrist::piece(piece, to);
    }

    return piece;
}

// Makes/unmakes a castling move.
template<uint32_t Flags>
INLINE void Board::makeCastlingMove(Move move) {
    // Move the king
    Piece king = this->movePiece<Flags>(move.from(), move.to());

    // Move the rook
    Square rookFrom = CastlingRook::from(king.color(), move.castlingSide());
    Square rookTo = CastlingRook::to(king.color(), move.castlingSide());
    this->movePiece<Flags>(rookFrom, rookTo);

    // Update castling rights
    if constexpr (!(Flags & MakeMoveFlags::Unmake) && (Flags & MakeMoveFlags::Gameplay)) {
        this->castlingRights<Flags>(this->castlingRights_.without(king.color()));
    }
}

// Makes/unmakes a promotion move.
template<uint32_t Flags>
INLINE void Board::makePromotionMove(Move move) {
    if constexpr (Flags & MakeMoveFlags::Unmake) { // Unmake
        Piece promotion = this->pieceAt(move.to());

        // Remove the promoted piece
        this->removePiece<Flags>(promotion, move.to());

        // Add the pawn
        this->addPiece<Flags>(Piece::pawn(promotion.color()), move.from());
    } else { // Make
        Piece pawn = this->pieceAt(move.from());

        // Remove the old pawn
        this->removePiece<Flags>(pawn, move.from());

        // Add the new piece
        Piece promotion(pawn.color(), move.promotion());
        this->addPiece<Flags>(promotion, move.to());
    }
}

// Makes/unmakes a quiet move.
template<uint32_t Flags>
INLINE void Board::makeQuietMove(Move move) {
    // Move/unmove the piece
    Piece piece = this->movePiece<Flags>(move.from(), move.to());

    // Update gameplay information if necessary (don't need to update if unmaking since unmaking will just pull the old values
    // from the MakeMoveInfo)
    if constexpr (!(Flags & MakeMoveFlags::Unmake) && (Flags & MakeMoveFlags::Gameplay)) {
        switch (piece.type()) {
            case PieceType::Pawn: {
                // Update en passant square if double pawn push
                if (move.isDoublePawnPush()) {
                    // TODO: There's probably a better way of getting the en passant square, but for now, just average the from
                    //  and to squares
                    Square enPassantSquare(move.to().file(), (move.to().rank() + move.from().rank()) / 2);
                    this->enPassantSquare<Flags>(enPassantSquare);
                }
                break;
            }

            case PieceType::Rook: {
                // Update castling rights if necessary
                this->maybeRevokeCastlingRightsForRookSquare<Flags>(move.from());
                break;
            }

            case PieceType::King: {
                // Update castling rights
                this->castlingRights<Flags>(this->castlingRights_.without(piece.color()));
                break;
            }

            default: break;
        }
    }
}

template<uint32_t Flags>
MakeMoveInfo Board::makeMove(Move move) {
    // Save old state to MakeMoveInfo, so we can unmake the move later
    uint64_t oldHash;
    if constexpr (Flags & MakeMoveFlags::Hash) {
        oldHash = this->hash();
    }

    CastlingRights oldCastlingRights;
    Square oldEnPassantSquare;
    if constexpr (Flags & MakeMoveFlags::Gameplay) {
        oldCastlingRights = this->castlingRights();
        oldEnPassantSquare = this->enPassantSquare();
    }

    // Update the three-fold repetition hashes
    uint32_t oldPliesSinceIrreversible;
    if constexpr (Flags & MakeMoveFlags::Repetition) {
        oldPliesSinceIrreversible = this->pliesSinceIrreversible_;
        this->updateRepetitionHashes<Flags>(move);
    }

    // Reset en passant square (if the move is a double pawn push, will be set to correct value later during makeQuietMove)
    this->enPassantSquare<Flags>(Square::Invalid);

    // Captures
    Piece captured = Piece::empty();
    if (move.isCapture()) {
        Square capturedSquare = move.capturedSquare();

        // Check if the move is a capture before getting the captured piece to avoid unnecessary calls to pieceAt
        // (avoids unnecessary memory reads)
        captured = this->pieceAt(capturedSquare);
        assert(captured.type() != PieceType::King);
        this->removePiece<Flags>(captured, capturedSquare);

        // If a rook is captured, update castling rights if necessary
        if (captured.type() == PieceType::Rook) {
            this->maybeRevokeCastlingRightsForRookSquare<Flags>(capturedSquare);
        }
    }

    // Make the move
    if (move.isCastle()) { // Castling
        this->makeCastlingMove<Flags>(move);
    } else if (move.isPromotion()) { // Promotions
        this->makePromotionMove<Flags>(move);
    } else { // Quiet moves
        this->makeQuietMove<Flags>(move);
    }

    // Switch the turn
    if constexpr (Flags & MakeMoveFlags::Hash) {
        this->hash_ ^= Zobrist::blackToMove();
    }
    if constexpr (Flags & MakeMoveFlags::Turn) {
        this->turn_ = ~this->turn_;
    }

    return { oldHash, oldPliesSinceIrreversible, oldCastlingRights, oldEnPassantSquare, captured };
}

template<uint32_t Flags>
void Board::unmakeMove(Move move, MakeMoveInfo info) {
    constexpr uint32_t UnmakeFlags = Flags | MakeMoveFlags::Unmake;

    // We don't need to do any hash, castling rights, or en passant square updates here since we just restore the old hash and
    // castling rights from MakeMoveInfo

    if (move.isCastle()) { // Castling
        this->makeCastlingMove<UnmakeFlags>(move);
    } else if (move.isPromotion()) { // Promotions
        this->makePromotionMove<UnmakeFlags>(move);
    } else { // Normal moves
        this->makeQuietMove<UnmakeFlags>(move);
    }

    if (move.isCapture()) { // Captures
        // Add the captured piece back
        this->addPiece<UnmakeFlags>(info.captured, move.capturedSquare());
    }

    // Restore the old repetition hashes
    if constexpr (Flags & MakeMoveFlags::Repetition) {
        this->repetitionHashes_.pop_back();
        this->pliesSinceIrreversible_ = info.oldPliesSinceIrreversible;
    }

    // Restore the old hash
    if constexpr (Flags & MakeMoveFlags::Hash) {
        this->hash_ = info.oldHash;
    }

    // Restore the old gameplay info
    if constexpr (Flags & MakeMoveFlags::Gameplay) {
        this->castlingRights_ = info.oldCastlingRights;
        this->enPassantSquare_ = info.oldEnPassantSquare;
    }
    if constexpr (Flags & MakeMoveFlags::Turn) {
        this->turn_ = ~this->turn_;
    }
}

template MakeMoveInfo Board::makeMove<MakeMoveType::All>(Move);
template MakeMoveInfo Board::makeMove<MakeMoveType::AllNoTurn>(Move);
template MakeMoveInfo Board::makeMove<MakeMoveType::BitboardsOnly>(Move);
template void Board::unmakeMove<MakeMoveType::All>(Move, MakeMoveInfo);
template void Board::unmakeMove<MakeMoveType::AllNoTurn>(Move, MakeMoveInfo);
template void Board::unmakeMove<MakeMoveType::BitboardsOnly>(Move, MakeMoveInfo);


// Makes/unmakes a null move.
MakeMoveInfo Board::makeNullMove() {
    uint64_t oldHash = this->hash_;
    uint32_t oldPliesSinceIrreversible = this->pliesSinceIrreversible_;
    CastlingRights oldCastlingRights = this->castlingRights_;
    Square oldEnPassantSquare = this->enPassantSquare_;

    // Reset en passant square
    this->enPassantSquare<MakeMoveType::AllNoTurn>(Square::Invalid);

    // Switch the turn
    this->hash_ ^= Zobrist::blackToMove();

    return { oldHash, oldPliesSinceIrreversible, oldCastlingRights, oldEnPassantSquare, Piece::empty() };
}

void Board::unmakeNullMove(MakeMoveInfo info) {
    // Restore the old hash and gameplay info
    this->hash_ = info.oldHash;
    this->castlingRights_ = info.oldCastlingRights;
    this->enPassantSquare_ = info.oldEnPassantSquare;
}

} // namespace FKTB
