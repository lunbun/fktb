#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <vector>

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

class FixedDepthSearcher;

class MovePriorityQueue {
public:
    struct Entry {
        Move move;
        // The score of the move, used for sorting.
        int32_t score;
    };

    MovePriorityQueue();

    void append(Square to, Piece piece);
    void append(Square to, Piece piece, Piece capturedPiece);
    void append(Move move);

    // Loads the hash move from the previous iteration if the previous iteration is not nullptr.
    void maybeLoadHashMoveFromPreviousIteration(Board &board, FixedDepthSearcher *previousIteration);

    [[nodiscard]] inline bool empty() const { return !this->hashMove_.has_value() && this->moves_.empty(); }

    [[nodiscard]] Move pop();

private:
    std::optional<Move> hashMove_;
    std::vector<Entry> moves_;
};
