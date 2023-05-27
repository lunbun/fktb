#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <vector>

#include "piece.h"
#include "inline.h"

class Board;

// @formatter:off
namespace MoveFlagNamespace {
    enum MoveFlag : uint8_t {
        Quiet       = 0b0000,
        Capture     = 0b0100
    };
}
using MoveFlag = MoveFlagNamespace::MoveFlag;
// @formatter:on

class Move {
public:
    // Invalid moves have the same from and to squares.
    INLINE constexpr static Move invalid() { return { 0, 0, MoveFlag::Quiet }; }

    // Creates a move from a UCI string.
    static Move fromUci(const std::string &uci);

    INLINE constexpr Move(Square from, Square to, MoveFlag flags) : bits_((flags << 12) | (to << 6) | from) { }

    // Returns the square that the piece was moved from.
    [[nodiscard]] INLINE constexpr Square from() const { return this->bits_ & 0x3F; }
    // Returns the square that the piece was moved to.
    [[nodiscard]] INLINE constexpr Square to() const { return (this->bits_ >> 6) & 0x3F; }
    // Returns the flags for the move.
    [[nodiscard]] INLINE constexpr MoveFlag flags() const { return static_cast<MoveFlag>((this->bits_ >> 12) & 0x0F); }

    [[nodiscard]] INLINE constexpr bool isValid() const { return this->from() != this->to(); }
    [[nodiscard]] INLINE constexpr bool isCapture() const { return this->flags() & MoveFlag::Capture; }

    [[nodiscard]] bool operator==(const Move &other) const;

    [[nodiscard]] std::string uci() const;
    [[nodiscard]] std::string debugName(const Board &board) const;

private:
    uint16_t bits_;
};

static_assert(sizeof(Move) == 2, "Move must be 2 bytes");

class ReadonlyTranspositionTable;

// A stack of move priority queues.
//
// We use this instead of std::vector because we want to avoid the overhead of constructing/destructing the queues (yes,
// using this instead of std::vector provides a significant 3x speedup in the search). Also, we want to avoid the
// overhead of copying the queues when we enqueue/dequeue. Since we perform a selection sort every time we dequeue an
// element, we don't need to worry about the order of moves in the queue and so both enqueue and dequeue can be O(1).
//
// We reuse the same memory buffer for all the queues. The theory for this I guess is that we improve cache locality by
// doing this, but I don't know if this actually helps.
class MovePriorityQueueStack {
public:
    MovePriorityQueueStack(uint16_t stackCapacity, uint16_t capacity);
    ~MovePriorityQueueStack();

    // Pushes/pops a move priority queue onto the top of the stack.
    void push();
    void pop();

    // Enqueues/dequeues a move onto the top of the stack.
    void enqueue(Move move);
    [[nodiscard]] Move dequeue();
    [[nodiscard]] bool empty() const;

    // Loads the hash move from the previous table if the previous table is not nullptr.
    void maybeLoadHashMoveFromPreviousIteration(Board &board, ReadonlyTranspositionTable *previousTable);

    // Scores the moves in the queue.
    template<Color Side>
    void score(const Board &board);

private:
    struct QueueEntry {
        Move move;
        // The score of the move, used for sorting.
        int32_t score;
    };

    struct QueueStackFrame {
        QueueEntry *queueStart;
        Move hashMove;

        QueueStackFrame() = delete;
    };

    QueueEntry *queueStart_;
    QueueEntry *queueEnd_;
    Move hashMove_;

    QueueStackFrame *stackStart_; // Owns the memory for the stack
    QueueStackFrame *stackIndex_;
    QueueStackFrame *stackEnd_;

    QueueEntry *movesStart_; // Owns the memory for the moves
    QueueEntry *movesEnd_;
};

// This class is used to ensure that the move priority queue stack is popped at the end of the function.
class MovePriorityQueueStackGuard {
public:
    INLINE explicit MovePriorityQueueStackGuard(MovePriorityQueueStack &stack) : stack_(stack) { this->stack_.push(); }
    INLINE ~MovePriorityQueueStackGuard() { this->stack_.pop(); }

private:
    MovePriorityQueueStack &stack_;
};
