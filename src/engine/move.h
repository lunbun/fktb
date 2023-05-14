#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <stdexcept>

#include "piece.h"

class Board;

class Move {
public:
    static Move fromString(const std::string &string, const Board &board);

    Move(Square to, Piece piece);
    Move(Square to, Piece piece, Piece capturedPiece);

    // Returns the square that the piece was moved from.
    [[nodiscard]] inline Square from() const {
        return { static_cast<int8_t>(this->fromFile_), static_cast<int8_t>(this->fromRank_) };
    }
    // Returns the square that the piece was moved to.
    [[nodiscard]] inline Square to() const {
        return { static_cast<int8_t>(this->toFile_), static_cast<int8_t>(this->toRank_) };
    }

    [[nodiscard]] inline PieceType pieceType() const { return static_cast<PieceType>(this->pieceType_); }
    [[nodiscard]] inline PieceColor pieceColor() const { return static_cast<PieceColor>(this->pieceColor_); }
    [[nodiscard]] Piece piece() const;

    [[nodiscard]] inline bool isCapture() const { return this->isCapture_; }

    // Returns the piece that was captured by this move.
    // These are only valid if isCapture() returns true.
    [[nodiscard]] inline PieceType capturedPieceType() const {
        return static_cast<PieceType>(this->capturedPieceType_);
    }
    [[nodiscard]] inline PieceColor capturedPieceColor() const {
        return static_cast<PieceColor>(this->capturedPieceColor_);
    }
    [[nodiscard]] Piece capturedPiece() const;

    [[nodiscard]] bool operator==(const Move &other) const;

    [[nodiscard]] std::string debugName() const;

private:
    uint32_t fromFile_ : 3;
    uint32_t fromRank_ : 3;

    uint32_t toFile_ : 3;
    uint32_t toRank_ : 3;

    uint32_t pieceType_ : 3;
    uint32_t pieceColor_ : 1;

    uint32_t isCapture_ : 1;
    uint32_t capturedPieceType_ : 3;
    uint32_t capturedPieceColor_ : 1;
};



// A stack of lists.
class MoveListStack {
public:
    explicit MoveListStack(uint32_t stackCapacity, uint32_t listCapacity);
    ~MoveListStack();

    MoveListStack(const MoveListStack &other) = delete;
    MoveListStack &operator=(const MoveListStack &other) = delete;
    MoveListStack(MoveListStack &&other) = default;
    MoveListStack &operator=(MoveListStack &&other) = default;

    [[nodiscard]] inline uint32_t size() const { return this->listSize_; }

    void push();
    void pop();

    void append(Square to, Piece piece);
    void append(Square to, Piece piece, Piece capturedPiece);
    void append(Move move);

    // Returns the move at the given index without checking if the index is valid. Only use this if you know the index
    // is valid (e.g. in loops).
    [[nodiscard]] inline Move unsafeAt(uint32_t index) const { return this->moves_[this->listStart_ + index]; }

    // Sets the move at the given index without checking if the index is valid. Only use this if you know the index is
    // valid (e.g. in loops). Also, it is a bad idea to manually modify the move list, so the only time this should be
    // used is for move sorting.
    inline void unsafeSet(uint32_t index, Move move) {
        this->moves_[this->listStart_ + index] = move;
    }

private:
    uint32_t stackCapacity_;
    uint32_t stackSize_;

    uint32_t listCapacity_;
    uint32_t listSize_;
    uint32_t listStart_;
    uint32_t *listSizes_;

    Move *moves_;
};
