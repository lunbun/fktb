#include "move_list.h"

#include <cstdint>
#include <algorithm>

#include "move.h"
#include "engine/board/piece.h"
#include "engine/board/board.h"
#include "engine/hash/transposition.h"

MovePriorityQueue::MovePriorityQueue(MoveEntry *start, MoveEntry *end) : hashMove_(Move::invalid()), start_(start),
                                                                         end_(end) { }

Move MovePriorityQueue::dequeue() {
    if (this->hashMove_.isValid()) {
        // Always check the hash move first
        Move move = this->hashMove_;
        this->hashMove_ = Move::invalid();
        return move;
    }

    // Find the move with the highest score
    MoveEntry *bestMove;
    int32_t bestScore = -INT32_MAX;

    for (MoveEntry *entry = this->start_; entry < this->end_; entry++) {
        if (entry->score > bestScore) {
            bestMove = entry;
            bestScore = entry->score;
        }
    }

    Move move = bestMove->move;

    // Because order does not matter in the buffer (since we sort it anyway), we can quickly remove a move from the
    // queue by copying the move at the end of the queue into its place and decrementing the queue size.
    *bestMove = *(--this->end_);

    return move;
}

bool MovePriorityQueue::empty() const {
    return !this->hashMove_.isValid() && this->end_ == this->start_;
}

// Removes the first instance of the move from the queue, if it exists.
void MovePriorityQueue::remove(Move move) {
    if (!move.isValid()) {
        return;
    }

    for (MoveEntry *entry = this->start_; entry < this->end_; entry++) {
        if (entry->move == move) {
            // Because order does not matter in the buffer (since we sort it anyway), we can quickly remove a move from
            // the queue by copying the move at the end of the queue into its place and decrementing the queue size.
            *entry = *(--this->end_);
            return;
        }
    }
}



RootMoveList::RootMoveList(MoveEntry *start, MoveEntry *end) : moves_(start, end) { }

Move RootMoveList::dequeue() {
    // Pop the last move off the list
    Move move = this->moves_.back().move;
    this->moves_.pop_back();
    return move;
}

bool RootMoveList::empty() const {
    return this->moves_.empty();
}

void RootMoveList::loadHashMove(const Board &board, const TranspositionTable &table) {
    TranspositionTable::Entry *entry = table.load(board.hash());

    if (entry == nullptr) {
        return;
    }

    // We first need to remove the hash move from the list if it is present
    for (auto it = this->moves_.begin(); it != this->moves_.end(); it++) {
        if (it->move == entry->bestMove()) {
            this->moves_.erase(it);
            break;
        }
    }

    this->moves_.push_back({ entry->bestMove(), 0 });
}

void RootMoveList::sort() {
    // Sort the moves from lowest to highest score since we pop from the back
    std::sort(this->moves_.begin(), this->moves_.end(), [](const MoveEntry &a, const MoveEntry &b) {
        return a.score < b.score;
    });
}
