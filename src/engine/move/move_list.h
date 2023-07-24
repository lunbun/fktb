#pragma once

#include <cstdint>
#include <vector>

#include "move.h"
#include "engine/inline.h"
#include "engine/board/piece.h"
#include "engine/board/board.h"

namespace FKTB {

class TranspositionTable;

constexpr uint32_t MaxMoveCount = 128;
constexpr uint32_t MaxTacticalCount = 32;

// Using uint64_t[] instead of MoveEntry[] allows the array to be uninitialized. It does mean though that we have to
// reinterpret_cast the array to MoveEntry * when we want to use it.
using AlignedMoveEntry = uint64_t;

struct MoveEntry {
    Move move;
    int32_t score;

    MoveEntry() = delete;

    INLINE static MoveEntry *fromAligned(AlignedMoveEntry *buffer) { return reinterpret_cast<MoveEntry *>(buffer); }
};

static_assert(sizeof(MoveEntry) == 8, "MoveEntry must be 8 bytes");


// Simple wrapper around a raw array of MoveEntries that allows us to push moves onto the end of the array.
//
// This does not actually allocate any memory, it just wraps a pointer to an array of MoveEntries (ideally on the array
// is on the stack).
class MoveList {
public:
    INLINE constexpr explicit MoveList(MoveEntry *entries) : pointer_(entries) { }

    INLINE void push(Move move) { *(this->pointer_++) = { move, 0 }; }

    [[nodiscard]] INLINE constexpr MoveEntry *pointer() const { return this->pointer_; }

private:
    MoveEntry *pointer_;
};

// A lightweight wrapper around a raw array of MoveEntries that provides a priority queue interface.
//
// This does not actually allocate any memory, it just wraps a pointer to an array of MoveEntries (ideally on the array
// is on the stack).
class MovePriorityQueue {
public:
    INLINE constexpr MovePriorityQueue(MoveEntry *start, MoveEntry *end) : start_(start), end_(end) { }

    // Dequeues the move with the highest score (performs a selection sort on the array).
    [[nodiscard]] Move dequeue();
    [[nodiscard]] INLINE bool empty() const { return this->start_ == this->end_; }

    // Removes the first instance of the move from the queue, if it exists.
    void remove(Move move);

    [[nodiscard]] INLINE MoveEntry *start() const { return this->start_; }
    [[nodiscard]] INLINE MoveEntry *end() const { return this->end_; }

private:
    MoveEntry *start_;
    MoveEntry *end_;
};

// This actually allocates memory on the heap, and sorts the moves ahead of time instead of using a priority queue.
//
// It's alright for the root move list to be slow because it's only used once per search.
class RootMoveList {
public:
    // Copies the entries.
    RootMoveList(MoveEntry *start, MoveEntry *end);

    // Returns the next move in the list.
    [[nodiscard]] Move dequeue();
    [[nodiscard]] bool empty() const;

    // Loads the hash move from the transposition table (if the hash move exists).
    //
    // Should be called after the root move list is sorted, otherwise the hash move will be sorted with the rest of the
    // moves.
    void loadHashMove(const Board &board, const TranspositionTable &table);

    // Sorts the moves in the list (assumes they are already scored).
    void sort();

    [[nodiscard]] INLINE auto &moves() { return this->moves_; }
    [[nodiscard]] INLINE const auto &moves() const { return this->moves_; }

private:
    std::vector<MoveEntry> moves_;
};

} // namespace FKTB
