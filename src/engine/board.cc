#include "board.h"

#include <cstdint>
#include <string>

#include "piece.h"
#include "fen.h"
#include "transposition.h"

Board::Board(Color turn) : material_(), turn_(turn), hash_(0), pieces_(), bitboards_() {
    this->pieces_.fill(Piece::empty());
    this->bitboards_.white().fill(0);
    this->bitboards_.black().fill(0);

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
    Board board(reader.turn());

    while (reader.hasNext()) {
        auto entry = reader.next();

        board.addPiece(entry.piece, entry.square);
    }

    return board;
}

std::string Board::toFen() const {
    FenWriter writer{};

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

    writer.setTurn(this->turn_);

    return writer.fen();
}

Board Board::copy() const {
    return Board::fromFen(this->toFen());
}



void Board::addPiece(Piece piece, Square square) {
    this->pieces_[square] = piece;
    this->bitboard(piece.type(), piece.color()).set(square);
    this->material_[piece.color()] += piece.material();
    this->hash_ ^= Zobrist::piece(piece, square);
}

void Board::removePiece(Piece piece, Square square) {
    this->pieces_[square] = Piece::empty();
    this->bitboard(piece.type(), piece.color()).clear(square);
    this->material_[piece.color()] -= piece.material();
    this->hash_ ^= Zobrist::piece(piece, square);
}

MakeMoveInfo Board::makeMoveNoTurnUpdate(Move move) {
    // Remove captured piece
    Piece captured = Piece::empty();

    if (move.isCapture()) {
        // Check if the move is a capture before getting the captured piece to avoid unnecessary calls to pieceAt
        // (avoids unnecessary memory reads)
        captured = this->pieceAt(move.to());
        this->removePiece(captured, move.to());
    }

    Piece piece = this->pieceAt(move.from());
    if (move.isPromotion()) {
        // Remove the old pawn
        this->removePiece(piece, move.from());

        // Add the new piece
        Piece promotion = Piece(move.promotion(), piece.color());
        this->addPiece(promotion, move.to());
    } else {
        // Move the piece
        this->pieces_[move.from()] = Piece::empty();
        this->pieces_[move.to()] = piece;

        Bitboard &bitboard = this->bitboard(piece.type(), piece.color());
        bitboard.clear(move.from());
        bitboard.set(move.to());

        this->hash_ ^= Zobrist::piece(piece, move.from());
        this->hash_ ^= Zobrist::piece(piece, move.to());
    }

    // Switch the turn
    this->hash_ ^= Zobrist::blackToMove();

    return { captured };
}

void Board::unmakeMoveNoTurnUpdate(Move move, MakeMoveInfo info) {
    // Remove the piece from its new square
    Piece piece = this->pieceAt(move.to());
    if (move.isPromotion()) {
        // Remove the promoted piece
        this->removePiece(piece, move.to());

        // Add the pawn
        this->addPiece(Piece::pawn(piece.color()), move.from());
    } else {
        // Unmove the piece
        this->pieces_[move.to()] = Piece::empty();
        this->pieces_[move.from()] = piece;

        Bitboard &bitboard = this->bitboard(piece.type(), piece.color());
        bitboard.clear(move.to());
        bitboard.set(move.from());

        this->hash_ ^= Zobrist::piece(piece, move.to());
        this->hash_ ^= Zobrist::piece(piece, move.from());
    }

    // Add captured piece
    if (!info.captured.isEmpty()) {
        this->addPiece(info.captured, move.to());
    }

    // Switch the turn
    this->hash_ ^= Zobrist::blackToMove();
}

MakeMoveInfo Board::makeMove(Move move) {
    this->turn_ = ~this->turn_;
    return this->makeMoveNoTurnUpdate(move);
}

void Board::unmakeMove(Move move, MakeMoveInfo info) {
    this->turn_ = ~this->turn_;
    this->unmakeMoveNoTurnUpdate(move, info);
}
