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



MovePriorityQueue::MovePriorityQueue() : moves_() { }

void MovePriorityQueue::append(Square to, Piece piece) {
    this->append(Move(to, piece));
}

void MovePriorityQueue::append(Square to, Piece piece, Piece capturedPiece) {
    this->append(Move(to, piece, capturedPiece));
}

void MovePriorityQueue::append(Move move) {
    if (move == this->hashMove_) {
        // The hash move is special and should not be added to the queue
        return;
    }

    this->moves_.push_back({ move, MoveSort::scoreMove(move) });
}

// Loads the hash move from the previous iteration if the previous iteration is not nullptr.
void MovePriorityQueue::maybeLoadHashMoveFromPreviousIteration(Board &board, FixedDepthSearcher *previousIteration) {
    if (previousIteration == nullptr) {
        return;
    }

    TranspositionTable::Entry *entry = previousIteration->table().load(board.hash());

    if (entry == nullptr) {
        return;
    }

    this->hashMove_ = entry->bestMove.value();
}

Move MovePriorityQueue::pop() {
    if (this->hashMove_.has_value()) {
        // Always search the hash move first
        Move move = this->hashMove_.value();
        this->hashMove_ = std::nullopt;
        return move;
    }

    // Find the move with the highest score
    auto bestMove = this->moves_.begin();
    for (auto it = this->moves_.begin(); it != this->moves_.end(); it++) {
        if (it->score > bestMove->score) {
            bestMove = it;
        }
    }

    // Remove the move from the queue
    Move move = bestMove->move;
    this->moves_.erase(bestMove);

    return move;
}
