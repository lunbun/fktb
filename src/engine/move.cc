#include "move.h"

#include <stdexcept>
#include <cstring>

#include "board.h"
#include "movesort.h"
#include "search.h"

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



MovePriorityQueueStack::MovePriorityQueueStack(uint32_t stackCapacity, uint32_t capacity) {
    this->stackCapacity_ = stackCapacity;
    this->stackSize_ = 0;
    this->stack_ = static_cast<QueueStackFrame *>(malloc(sizeof(QueueStackFrame) * stackCapacity));

    this->capacity_ = capacity;
    this->moves_ = static_cast<QueueEntry *>(malloc(sizeof(QueueEntry) * capacity));

    this->queueStart_ = this->moves_;
    this->queueSize_ = 0;
    this->hashMove_ = std::nullopt;
}

MovePriorityQueueStack::~MovePriorityQueueStack() {
    free(this->stack_);
    free(this->moves_);
}

void MovePriorityQueueStack::push() {
    if (this->stackSize_ >= this->stackCapacity_) {
        throw std::runtime_error("MovePriorityQueueStack stack is full");
    }

    this->stack_[this->stackSize_++] = { this->queueStart_, this->hashMove_ };

    this->queueStart_ += this->queueSize_;
    this->queueSize_ = 0;
    this->hashMove_ = std::nullopt;
}

void MovePriorityQueueStack::pop() {
    if (this->stackSize_ <= 0) {
        throw std::runtime_error("MovePriorityQueueStack stack is empty");
    }

    // The end of the previous queue is the start of the current queue
    QueueEntry *queueEnd = this->queueStart_;

    QueueStackFrame top = this->stack_[--this->stackSize_];
    this->queueStart_ = top.queueStart;
    this->queueSize_ = queueEnd - top.queueStart;
    this->hashMove_ = top.hashMove;
}

void MovePriorityQueueStack::enqueue(Square to, Piece piece) {
    this->enqueue(Move(to, piece));
}

void MovePriorityQueueStack::enqueue(Square to, Piece piece, Piece capturedPiece) {
    this->enqueue(Move(to, piece, capturedPiece));
}

void MovePriorityQueueStack::enqueue(Move move) {
    if (this->queueSize_ >= this->capacity_) {
        throw std::runtime_error("MovePriorityQueueStack queue is full");
    }

    if (move == this->hashMove_) {
        return;
    }

    this->queueStart_[this->queueSize_++] = { move, MoveSort::scoreMove(move) };
}

// Loads the hash move from the previous iteration if the previous iteration is not nullptr.
void MovePriorityQueueStack::maybeLoadHashMoveFromPreviousIteration(Board &board, FixedDepthSearcher *previousIteration) {
    if (previousIteration == nullptr) {
        return;
    }

    TranspositionTable::Entry *entry = previousIteration->table().load(board.hash());

    if (entry == nullptr) {
        return;
    }

    this->hashMove_ = entry->bestMove.value();
}

Move MovePriorityQueueStack::dequeue() {
    if (this->hashMove_.has_value()) {
        // Always search the hash move first
        Move move = this->hashMove_.value();
        this->hashMove_ = std::nullopt;
        return move;
    }

    // Find the move with the highest score
    QueueEntry *bestMove;
    int32_t bestScore = -INT32_MAX;

    QueueEntry *queueEnd = this->queueStart_ + this->queueSize_;
    for (QueueEntry *entry = this->queueStart_; entry < queueEnd; entry++) {
        if (entry->score > bestScore) {
            bestMove = entry;
            bestScore = entry->score;
        }
    }

    Move move = bestMove->move;

    // Because order does not matter in the buffer (since we sort it anyway), we can quickly remove a move from the
    // queue by copying the move at the end of the queue into its place and decrementing the queue size.
    *bestMove = *(queueEnd - 1);
    this->queueSize_--;

    return move;
}

bool MovePriorityQueueStack::empty() const {
    return !this->hashMove_.has_value() && this->queueSize_ == 0;
}



MovePriorityQueueStackGuard::MovePriorityQueueStackGuard(MovePriorityQueueStack &stack) : stack_(stack) {
    this->stack_.push();
}

MovePriorityQueueStackGuard::~MovePriorityQueueStackGuard() {
    this->stack_.pop();
}
