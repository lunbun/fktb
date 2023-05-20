#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <vector>

#include "piece.h"

class Board;

class Move {
public:
    static Move fromUci(const std::string &uci, const Board &board);

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

    [[nodiscard]] std::string uci() const;
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

class MovePriorityQueueStack {
public:
    MovePriorityQueueStack(uint16_t stackCapacity, uint16_t capacity);
    ~MovePriorityQueueStack();

    // Pushes/pops a move priority queue onto the top of the stack.
    void push();
    void pop();

    // Enqueues/dequeues a move onto the top of the stack.
    void enqueue(Square to, Piece piece);
    void enqueue(Square to, Piece piece, Piece capturedPiece);
    void enqueue(Move move);
    [[nodiscard]] Move dequeue();
    [[nodiscard]] bool empty() const;

    // Loads the hash move from the previous iteration if the previous iteration is not nullptr.
    void maybeLoadHashMoveFromPreviousIteration(Board &board, FixedDepthSearcher *previousIteration);

private:
    struct QueueEntry {
        Move move;
        // The score of the move, used for sorting.
        int32_t score;
    };

    struct QueueStackFrame {
        QueueEntry *queueStart;
        std::optional<Move> hashMove;

        QueueStackFrame() = delete;
    };

    QueueEntry *queueStart_;
    uint16_t queueSize_;
    std::optional<Move> hashMove_;

    uint16_t stackCapacity_;
    uint16_t stackSize_;
    QueueStackFrame *stack_;

    uint16_t capacity_;
    QueueEntry *moves_;
};

// This class is used to ensure that the move priority queue stack is popped at the end of the function.
class MovePriorityQueueStackGuard {
public:
    explicit MovePriorityQueueStackGuard(MovePriorityQueueStack &stack);
    ~MovePriorityQueueStackGuard();

private:
    MovePriorityQueueStack &stack_;
};
