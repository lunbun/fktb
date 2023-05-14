#include "move.h"

#include <stdexcept>
#include <cstring>

#include "board.h"

Move Move::fromString(const std::string &string, const Board &board) {
    if (string.length() != 4) {
        throw std::invalid_argument("Move string must be 4 characters long");
    }

    auto fromFile = static_cast<int8_t>(string[0] - 'a');
    auto fromRank = static_cast<int8_t>(string[1] - '1');
    auto toFile = static_cast<int8_t>(string[2] - 'a');
    auto toRank = static_cast<int8_t>(string[3] - '1');

    Piece *piece = board.getPieceAt({ fromFile, fromRank });
    Piece *capturedPiece = board.getPieceAt({ toFile, toRank });

    if (capturedPiece == nullptr) {
        return Move({ toFile, toRank }, *piece);
    } else {
        return Move({ toFile, toRank }, *piece, *capturedPiece);
    }
}

Move::Move(Square to, Piece piece) {
    this->fromFile_ = static_cast<uint8_t>(piece.file());
    this->fromRank_ = static_cast<uint8_t>(piece.rank());

    this->toFile_ = static_cast<uint8_t>(to.file);
    this->toRank_ = static_cast<uint8_t>(to.rank);

    this->pieceType_ = static_cast<uint8_t>(piece.type());
    this->pieceColor_ = static_cast<uint8_t>(piece.color());

    this->isCapture_ = false;
    this->capturedPieceType_ = 0;
    this->capturedPieceColor_ = 0;
}

Move::Move(Square to, Piece piece, Piece capturedPiece) {
    this->fromFile_ = static_cast<uint8_t>(piece.file());
    this->fromRank_ = static_cast<uint8_t>(piece.rank());

    this->toFile_ = static_cast<uint8_t>(to.file);
    this->toRank_ = static_cast<uint8_t>(to.rank);

    this->pieceType_ = static_cast<uint8_t>(piece.type());
    this->pieceColor_ = static_cast<uint8_t>(piece.color());

    this->isCapture_ = true;
    this->capturedPieceType_ = static_cast<uint8_t>(capturedPiece.type());
    this->capturedPieceColor_ = static_cast<uint8_t>(capturedPiece.color());
}



Piece Move::piece() const {
    return { static_cast<PieceType>(this->pieceType_), static_cast<PieceColor>(this->pieceColor_), this->from() };
}

Piece Move::capturedPiece() const {
    return {
        static_cast<PieceType>(this->capturedPieceType_),
        static_cast<PieceColor>(this->capturedPieceColor_),
        this->to()
    };
}

bool Move::operator==(const Move &other) const {
    return !memcmp(this, &other, sizeof(Move));
}

std::string Move::debugName() const {
    std::string name;

    name += this->piece().debugName();
    name += " from ";
    name += this->from().debugName();
    name += " to ";
    name += this->to().debugName();

    if (this->isCapture()) {
        name += " capturing ";
        name += this->capturedPiece().debugName();
    }

    return name;
}



MoveListStack::MoveListStack(uint32_t stackCapacity, uint32_t listCapacity) : stackCapacity_(stackCapacity),
                                                                              stackSize_(0),
                                                                              listCapacity_(listCapacity), listSize_(0),
                                                                              listStart_(0) {
    this->listSizes_ = static_cast<uint32_t *>(malloc(sizeof(uint32_t) * stackCapacity));
    this->moves_ = static_cast<Move *>(malloc(sizeof(Move) * listCapacity));
}

MoveListStack::~MoveListStack() {
    free(this->listSizes_);
    free(this->moves_);
}

void MoveListStack::push() {
    if (this->stackSize_ >= this->stackCapacity_) {
        throw std::runtime_error("MoveListStack::push() called when stack is full");
    }

    this->listSizes_[this->stackSize_] = this->listSize_;
    this->stackSize_++;

    this->listStart_ += this->listSize_;
    this->listSize_ = 0;
}

void MoveListStack::pop() {
    if (this->stackSize_ <= 0) {
        throw std::runtime_error("MoveListStack::pop() called when stack is empty");
    }

    this->stackSize_--;

    this->listSize_ = this->listSizes_[this->stackSize_];
    this->listStart_ -= this->listSize_;
}

void MoveListStack::append(Square to, Piece piece) {
    this->append(Move(to, piece));
}

void MoveListStack::append(Square to, Piece piece, Piece capturedPiece) {
    this->append(Move(to, piece, capturedPiece));
}

void MoveListStack::append(Move move) {
    if (this->listStart_ + this->listSize_ >= this->listCapacity_) {
        throw std::runtime_error("MoveListStack::append() called when list is full");
    }

    this->moves_[this->listStart_ + this->listSize_] = move;
    this->listSize_++;
}
