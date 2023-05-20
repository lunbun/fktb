#include "board.h"

#include <cstdint>
#include <string>

#include "piece.h"
#include "fen.h"

Board::Board(PieceColor turn) : material_(), turn_(turn), hash_(turn), pawns_(), knights_(), bishops_(), rooks_(),
                                queens_(), kings_(), squares_() {
    this->squares_.fill(nullptr);
}

Board::~Board() = default;

Board Board::startingPosition() {
    return Board::fromFen(Board::STARTING_FEN);
}

Board Board::fromFen(const std::string &fen) {
    FenReader reader(fen);
    Board board(reader.turn());

    while (reader.hasNext()) {
        Piece piece = reader.next();

        board.addPiece(piece);
    }

    return board;
}

std::string Board::toFen() const {
    FenWriter writer{};

    for (int8_t rank = 7; rank >= 0; rank--) {
        for (int8_t file = 0; file < 8; file++) {
            Piece *piece = this->getPieceAt({ file, rank });
            if (piece == nullptr) {
                writer.empty();
            } else {
                writer.piece(*piece);
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

std::vector<std::unique_ptr<Piece>> &Board::getPieceList(Piece piece) {
    ColorMap<std::vector<std::unique_ptr<Piece>>> *list = nullptr;
    switch (piece.type()) {
        case PieceType::Pawn:
            list = &this->pawns_;
            break;
        case PieceType::Knight:
            list = &this->knights_;
            break;
        case PieceType::Bishop:
            list = &this->bishops_;
            break;
        case PieceType::Rook:
            list = &this->rooks_;
            break;
        case PieceType::Queen:
            list = &this->queens_;
            break;
        case PieceType::King:
            list = &this->kings_;
            break;
    }

    return (*list)[piece.color()];
}

void Board::addPieceNoHashUpdate(Piece piece) {
    auto &pieces = this->getPieceList(piece);

    pieces.push_back(std::make_unique<Piece>(piece));
    this->squares_[piece.square().index()] = pieces.back().get();

    this->material_[piece.color()] += piece.material();
}

void Board::removePieceNoHashUpdate(Piece *piece) {
    this->material_[piece->color()] -= piece->material();

    this->squares_[piece->square().index()] = nullptr;

    // Find the piece in the list and remove it.
    auto &pieces = this->getPieceList(*piece);

    for (auto it = pieces.begin(); it != pieces.end(); it++) {
        if (it->get() == piece) {
            pieces.erase(it);
            break;
        }
    }
}

void Board::addPiece(Piece piece) {
    this->addPieceNoHashUpdate(piece);
    this->hash_.piece(piece);
}

void Board::removePiece(Piece *piece) {
    this->hash_.piece(*piece);
    this->removePieceNoHashUpdate(piece);
}

Piece *Board::getPieceAt(Square square) const {
    return this->squares_[square.index()];
}

void Board::makeMove(Move move) {
    // Remove captured piece
    Piece *capturedPiece = this->getPieceAt(move.to());
    if (capturedPiece != nullptr) {
        this->removePieceNoHashUpdate(capturedPiece);
    }

    Piece *piece = this->getPieceAt(move.from());

    piece->setSquare(move.to());

    this->squares_[move.from().index()] = nullptr;
    this->squares_[move.to().index()] = piece;

    this->turn_ = ~this->turn_;

    this->hash_.move(move);
}

void Board::unmakeMove(Move move) {
    Piece *piece = this->getPieceAt(move.to());

    piece->setSquare(move.from());

    this->squares_[move.from().index()] = piece;
    this->squares_[move.to().index()] = nullptr;

    if (move.isCapture()) {
        this->addPieceNoHashUpdate(move.capturedPiece());
    }

    this->turn_ = ~this->turn_;

    this->hash_.move(move);
}
