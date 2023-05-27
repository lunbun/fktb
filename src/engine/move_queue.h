#pragma once

#include <cstdint>

#include "move.h"
#include "piece.h"
#include "board.h"

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
