#include "fen.h"

#include <stdexcept>
#include <optional>
#include <sstream>

namespace {
    std::vector<std::string> split(const std::string &s, char delim) {
        std::vector<std::string> result;
        std::stringstream ss(s);
        std::string item;

        while (getline(ss, item, delim)) {
            result.push_back(item);
        }

        return result;
    }
}

FenReader::FenReader(const std::string &fen) : file_(0), rank_(7) {
    this->fields_ = split(fen, ' ');
    if (this->fields_.empty()) {
        throw std::runtime_error("Invalid FEN string");
    }

    this->pieceField_ = this->fields_[0];
    this->pieceIndex_ = 0;

    this->readNextPiece();
}

// @formatter:off
PieceType fenCharToPieceType(char c) {
    switch (c) {
        case 'p': return PieceType::Pawn;
        case 'n': return PieceType::Knight;
        case 'b': return PieceType::Bishop;
        case 'r': return PieceType::Rook;
        case 'q': return PieceType::Queen;
        case 'k': return PieceType::King;
        default: throw std::runtime_error("Invalid piece type");
    }
}
// @formatter:on

FenReader::Entry fenCharToPiece(char c, Square square) {
    if (isupper(c)) {
        return { Piece::white(fenCharToPieceType(static_cast<char>(tolower(c)))), square };
    } else {
        return { Piece::black(fenCharToPieceType(c)), square };
    }
}

bool FenReader::hasNext() const {
    return this->nextPiece_.has_value();
}

FenReader::Entry FenReader::next() {
    if (!this->nextPiece_.has_value()) {
        throw std::runtime_error("No next entry in FEN string");
    }
    Entry entry = this->nextPiece_.value();

    this->readNextPiece();

    return entry;
}

void FenReader::readNextPiece() {
    this->nextPiece_ = std::nullopt;

    while (this->pieceIndex_ < this->pieceField_.size() && !this->nextPiece_.has_value()) {
        char c = this->pieceField_[this->pieceIndex_];

        if (c == '/') {
            this->rank_--;
            this->file_ = 0;
        } else if (isdigit(c)) {
            this->file_ += static_cast<int8_t>(c - '0');
        } else {
            this->nextPiece_ = fenCharToPiece(c, Square(this->file_, this->rank_ ));
            this->file_++;
        }

        this->pieceIndex_++;
    }
}

Color FenReader::turn() const {
    if (this->fields_.size() < 2) {
        return Color::White;
    }

    const std::string &turn = this->fields_[1];
    if (turn == "w") {
        return Color::White;
    } else if (turn == "b") {
        return Color::Black;
    } else {
        throw std::runtime_error("Invalid turn in FEN string");
    }
}



FenWriter::FenWriter() : rank_(7), file_(0), emptyFilesInARow_(0), fen_() { }

// @formatter:off
char pieceTypeToFenChar(PieceType type) {
    switch (type) {
        case PieceType::Pawn: return 'p';
        case PieceType::Knight: return 'n';
        case PieceType::Bishop: return 'b';
        case PieceType::Rook: return 'r';
        case PieceType::Queen: return 'q';
        case PieceType::King: return 'k';
        default: throw std::runtime_error("Invalid piece type");
    }
}
// @formatter:on

char pieceToFenChar(Piece piece) {
    if (piece.color() == Color::White) {
        return static_cast<char>(toupper(pieceTypeToFenChar(piece.type())));
    } else {
        return static_cast<char>(tolower(pieceTypeToFenChar(piece.type())));
    }
}

void FenWriter::piece(Piece piece) {
    this->maybeAddEmptyFilesToFen();
    this->fen_ += pieceToFenChar(piece);

    this->file_++;
}

void FenWriter::empty() {
    this->emptyFilesInARow_++;

    this->file_++;
}

void FenWriter::nextRank() {
    this->maybeAddEmptyFilesToFen();
    // If the rank is not the last rank, add a '/' (makes sure that we only add a slash BETWEEN ranks, not after ranks)
    if (this->rank_ > 0) {
        this->fen_ += '/';
    }

    this->file_ = 0;
    this->rank_--;
}

void FenWriter::setTurn(Color color) {
    this->fen_ += ' ';
    if (color == Color::White) {
        this->fen_ += 'w';
    } else {
        this->fen_ += 'b';
    }
}

void FenWriter::maybeAddEmptyFilesToFen() {
    if (this->emptyFilesInARow_ > 0) {
        this->fen_ += std::to_string(this->emptyFilesInARow_);
        this->emptyFilesInARow_ = 0;
    }
}
