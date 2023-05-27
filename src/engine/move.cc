#include "move.h"

#include <stdexcept>
#include <cstring>
#include <cstdlib>

#include "board.h"
#include "move_score.h"
#include "transposition.h"

Move Move::fromUci(const std::string &uci) {
    if (uci.length() != 4) {
        throw std::invalid_argument("Move string must be 4 characters long");
    }

    auto fromFile = static_cast<int8_t>(uci[0] - 'a');
    auto fromRank = static_cast<int8_t>(uci[1] - '1');
    auto toFile = static_cast<int8_t>(uci[2] - 'a');
    auto toRank = static_cast<int8_t>(uci[3] - '1');

    return { Square(fromFile, fromRank), Square(toFile, toRank), MoveFlag::Quiet };
}



bool Move::operator==(const Move &other) const {
    return !std::memcmp(this, &other, sizeof(Move));
}

std::string Move::uci() const {
    std::string uci;

    uci += this->from().uci();
    uci += this->to().uci();

    return uci;
}

std::string Move::debugName(const Board &board) const {
    std::string name;

    Piece piece = board.pieceAt(this->from());
    name += piece.debugName();
    name += " from ";
    name += this->from().debugName();
    name += " to ";
    name += this->to().debugName();

    Piece captured = board.pieceAt(this->to());
    if (!captured.isEmpty()) {
        name += " capturing ";
        name += captured.debugName();
    }

    return name;
}



MovePriorityQueueStack::MovePriorityQueueStack(uint16_t stackCapacity, uint16_t capacity) : hashMove_(Move::invalid()) {
    this->stackStart_ = static_cast<QueueStackFrame *>(std::malloc(sizeof(QueueStackFrame) * stackCapacity));
    this->stackIndex_ = this->stackStart_;
    this->stackEnd_ = this->stackStart_ + stackCapacity;

    this->movesStart_ = static_cast<QueueEntry *>(std::malloc(sizeof(QueueEntry) * capacity));
    this->movesEnd_ = this->movesStart_ + capacity;

    this->queueStart_ = this->movesStart_;
    this->queueEnd_ = this->movesStart_;
}

MovePriorityQueueStack::~MovePriorityQueueStack() {
    std::free(this->stackStart_);
    std::free(this->movesStart_);
}

void MovePriorityQueueStack::push() {
    if (this->stackIndex_ >= this->stackEnd_) {
        throw std::runtime_error("MovePriorityQueueStack stack is full");
    }

    *(this->stackIndex_++) = { this->queueStart_, this->hashMove_ };

    this->queueStart_ = this->queueEnd_;
    this->hashMove_ = Move::invalid();
}

void MovePriorityQueueStack::pop() {
    if (this->stackIndex_ <= this->stackStart_) {
        throw std::runtime_error("MovePriorityQueueStack stack is empty");
    }

    this->stackIndex_--;

    this->queueEnd_ = this->queueStart_;
    this->queueStart_ = this->stackIndex_->queueStart;
    this->hashMove_ = this->stackIndex_->hashMove;
}

void MovePriorityQueueStack::enqueue(Move move) {
    if (this->queueEnd_ >= this->movesEnd_) {
        throw std::runtime_error("MovePriorityQueueStack queue is full");
    }

    if (move == this->hashMove_) {
        return;
    }

    *(this->queueEnd_++) = { move, 0 };
}

Move MovePriorityQueueStack::dequeue() {
    if (this->hashMove_.isValid()) {
        // Always search the hash move first
        Move move = this->hashMove_;
        this->hashMove_ = Move::invalid();
        return move;
    }

    // Find the move with the highest score
    QueueEntry *bestMove;
    int32_t bestScore = -INT32_MAX;

    for (QueueEntry *entry = this->queueStart_; entry < this->queueEnd_; entry++) {
        if (entry->score > bestScore) {
            bestMove = entry;
            bestScore = entry->score;
        }
    }

    Move move = bestMove->move;

    // Because order does not matter in the buffer (since we sort it anyway), we can quickly remove a move from the
    // queue by copying the move at the end of the queue into its place and decrementing the queue size.
    *bestMove = *(--this->queueEnd_);

    return move;
}

bool MovePriorityQueueStack::empty() const {
    return !this->hashMove_.isValid() && this->queueEnd_ == this->queueStart_;
}

// Loads the hash move from the previous iteration if the previous iteration is not nullptr.
void MovePriorityQueueStack::maybeLoadHashMoveFromPreviousIteration(Board &board, ReadonlyTranspositionTable *previousTable) {
    if (previousTable == nullptr) {
        return;
    }

    TranspositionTable::Entry *entry = previousTable->load(board.hash());

    if (entry == nullptr) {
        return;
    }

    this->hashMove_ = entry->bestMove();
}

// Scores the moves in the queue.
template<Color Side>
void MovePriorityQueueStack::score(const Board &board) {
    MoveScorer<Side> scorer(board);

    for (QueueEntry *entry = this->queueStart_; entry < this->queueEnd_; entry++) {
        entry->score = scorer.score(entry->move);
    }
}

template void MovePriorityQueueStack::score<Color::White>(const Board &board);
template void MovePriorityQueueStack::score<Color::Black>(const Board &board);
