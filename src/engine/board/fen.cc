#include "fen.h"

#include <stdexcept>
#include <optional>
#include <sstream>

#include "engine/inline.h"

namespace FKTB {

namespace {

INLINE std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim)) {
        result.push_back(item);
    }

    return result;
}

// @formatter:off
INLINE PieceType fenCharToPieceType(char c) {
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

INLINE FenReader::Entry fenCharToPiece(char c, Square square) {
    if (isupper(c)) {
        return { Piece::white(fenCharToPieceType(static_cast<char>(tolower(c)))), square };
    } else {
        return { Piece::black(fenCharToPieceType(c)), square };
    }
}

// @formatter:off
INLINE char pieceTypeToFenChar(PieceType type) {
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

INLINE char pieceToFenChar(Piece piece) {
    if (piece.color() == Color::White) {
        return static_cast<char>(toupper(pieceTypeToFenChar(piece.type())));
    } else {
        return static_cast<char>(tolower(pieceTypeToFenChar(piece.type())));
    }
}

} // namespace



FenReader::FenReader(const std::string &fen) : file_(0), rank_(7) {
    this->fields_ = split(fen, ' ');
    if (this->fields_.empty()) {
        throw std::runtime_error("Invalid FEN string");
    }

    this->pieceField_ = this->fields_[0];
    this->pieceIndex_ = 0;

    this->readNextPiece();
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
            this->nextPiece_ = fenCharToPiece(c, Square(this->file_, this->rank_));
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

CastlingRights FenReader::castlingRights() const {
    if (this->fields_.size() < 3) {
        return CastlingRights::All;
    }

    const std::string &castlingRights = this->fields_[2];
    if (castlingRights == "-") {
        return CastlingRights::None;
    }

    CastlingRights result = CastlingRights::None;
    for (char c : castlingRights) {
        // @formatter:off
        switch (c) {
            case 'K': result |= CastlingRights::WhiteKingSide; break;
            case 'Q': result |= CastlingRights::WhiteQueenSide; break;
            case 'k': result |= CastlingRights::BlackKingSide; break;
            case 'q': result |= CastlingRights::BlackQueenSide; break;
            default: throw std::runtime_error("Invalid castling rights in FEN string");
        }
        // @formatter:on
    }

    return result;
}

Square FenReader::enPassantSquare() const {
    if (this->fields_.size() < 4) {
        return Square::Invalid;
    }

    const std::string &enPassantSquare = this->fields_[3];
    if (enPassantSquare == "-") {
        return Square::Invalid;
    }

    return Square::fromFen(this->fields_[3]);
}



FenWriter::FenWriter() : rank_(7), file_(0), emptyFilesInARow_(0), fen_() { }

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

void FenWriter::turn(Color color) {
    this->fen_ += ' ';
    if (color == Color::White) {
        this->fen_ += 'w';
    } else {
        this->fen_ += 'b';
    }
}

void FenWriter::castlingRights(CastlingRights castlingRights) {
    this->fen_ += ' ';

    if (castlingRights == CastlingRights::None) {
        this->fen_ += '-';
        return;
    }

    // @formatter:off
    if (castlingRights.has(CastlingRights::WhiteKingSide))  this->fen_ += 'K';
    if (castlingRights.has(CastlingRights::WhiteQueenSide)) this->fen_ += 'Q';
    if (castlingRights.has(CastlingRights::BlackKingSide))  this->fen_ += 'k';
    if (castlingRights.has(CastlingRights::BlackQueenSide)) this->fen_ += 'q';
    // @formatter:on
}

void FenWriter::enPassantSquare(Square square) {
    this->fen_ += ' ';

    if (!square.isValid()) {
        this->fen_ += '-';
        return;
    }

    this->fen_ += square.fen();
}

void FenWriter::maybeAddEmptyFilesToFen() {
    if (this->emptyFilesInARow_ > 0) {
        this->fen_ += std::to_string(this->emptyFilesInARow_);
        this->emptyFilesInARow_ = 0;
    }
}

} // namespace FKTB
