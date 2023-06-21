#include "move_stream.h"

#include "move_ordering.h"
#include "engine/move/movegen.h"
#include "engine/move/legality_check.h"

// Bug with clang-tidy apparently, it wants me to insert "const" before the MoveStream::Stage return type, but that would be
// invalid C++.
// NOLINTNEXTLINE
MoveStream::Stage operator++(MoveStream::Stage &stage, int) {
    return stage = static_cast<MoveStream::Stage>(static_cast<uint8_t>(stage) + 1);
}

// Do not initialize moveBuffer_ here, it does not need to be initialized.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
MoveStream::MoveStream(Board &board, Move hashMove, const HeuristicTables &heuristics, uint16_t depth)
    : stage_(Stage::HashMove), board_(board), hashMove_(hashMove), queue_(nullptr, nullptr), heuristics_(heuristics),
      depth_(depth), killerIndex_(0) { }

template<Color Side>
Move MoveStream::next() {
    Board &board = this->board_;

    switch (this->stage_) {
        case Stage::HashMove: {
            this->stage_++;

            if (this->hashMove_.isValid()) {
                // Ensure the hash move is legal in case of rare hash key collisions (see
                // https://www.chessprogramming.org/Transposition_Table#KeyCollisions).
                LegalityChecker<Side> legalityChecker(board);

                if (legalityChecker.isLegal(this->hashMove_)) {
                    return this->hashMove_;
                }
            }

            [[fallthrough]];
        }

        case Stage::TacticalGeneration: {
            this->stage_++;

            MoveEntry *movesStart = MoveEntry::fromAligned(this->moveBuffer_);
            MoveEntry *movesEnd = MoveGeneration::generate<Side, MoveGeneration::Type::Tactical>(board, movesStart);

            this->queue_ = MovePriorityQueue(movesStart, movesEnd);

            // Remove the hash move from the move list if necessary, since we already searched it.
            if (this->hashMove_.isValid() && this->hashMove_.isTactical()) {
                this->queue_.remove(this->hashMove_);
            }

            // Score the moves in the queue.
            MoveOrdering::score<Side, MoveOrdering::Type::Tactical>(this->queue_, board, nullptr);

            [[fallthrough]];
        }

        case Stage::Tactical: {
            if (!this->queue_.empty()) {
                return this->queue_.dequeue();
            }

            this->stage_++;

            [[fallthrough]];
        }

        case Stage::Killers: {
            while (this->killerIndex_ < KillerTable::MaxKillerMoves) {
                Move killer = this->heuristics_.killers[this->depth_][this->killerIndex_++];

                if (killer.isValid()) {
                    // Ensure the killer move is legal.
                    LegalityChecker<Side> legalityChecker(board);

                    if (legalityChecker.isLegal(killer)) {
                        return killer;
                    }
                }
            }

            this->stage_++;

            [[fallthrough]];
        }

        case Stage::QuietGeneration: {
            this->stage_++;

            MoveEntry *movesStart = MoveEntry::fromAligned(this->moveBuffer_);
            MoveEntry *movesEnd = MoveGeneration::generate<Side, MoveGeneration::Type::Quiet>(board, movesStart);

            this->queue_ = MovePriorityQueue(movesStart, movesEnd);

            // Remove the hash move and killer moves from the move list if necessary, since we already searched them.
            if (this->hashMove_.isValid() && this->hashMove_.isQuiet()) {
                this->queue_.remove(this->hashMove_);
            }
            for (Move killer : this->heuristics_.killers[this->depth_]) {
                if (killer.isValid()) {
                    this->queue_.remove(killer);
                }
            }

            // Score the moves in the queue.
            MoveOrdering::score<Side, MoveOrdering::Type::Quiet>(this->queue_, board, &this->heuristics_.history);

            [[fallthrough]];
        }

        case Stage::Quiet: {
            if (!this->queue_.empty()) {
                return this->queue_.dequeue();
            }

            this->stage_++;

            [[fallthrough]];
        }

        case Stage::End: {
            return Move::invalid();
        }
    }

    assert(false);
    return Move::invalid();
}

template Move MoveStream::next<Color::White>();
template Move MoveStream::next<Color::Black>();
