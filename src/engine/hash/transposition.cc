#include "transposition.h"

#include <cstdlib>
#include <cstdint>
#include <random>
#include <array>
#include <cassert>

#include "engine/move/move.h"
#include "engine/board/square.h"
#include "engine/board/piece.h"

namespace {
    bool isInitialized = false;

    uint64_t blackToMoveNumber = 0;
    std::array<uint64_t, 16> castlingRightsNumbers;
    std::array<uint64_t, 9> enPassantFileNumbers; // Index 0 is for no en passant.
    ColorMap<PieceTypeMap<SquareMap<uint64_t>>> pieceNumbers;
}

void Zobrist::maybeInit() {
    constexpr uint64_t seed = 0;

    if (isInitialized) {
        return;
    }

    isInitialized = true;

    std::mt19937_64 generator(seed);

    blackToMoveNumber = generator();

    for (uint64_t &castlingRightsNumber : castlingRightsNumbers) {
        castlingRightsNumber = generator();
    }

    for (uint64_t &enPassantFileNumber : enPassantFileNumbers) {
        enPassantFileNumber = generator();
    }

    for (uint8_t piece = 0; piece < 6; piece++) {
        for (uint8_t square = 0; square < 64; square++) {
            pieceNumbers.white()[static_cast<PieceType>(piece)][square] = generator();
            pieceNumbers.black()[static_cast<PieceType>(piece)][square] = generator();
        }
    }
}

uint64_t Zobrist::blackToMove() {
    return blackToMoveNumber;
}

uint64_t Zobrist::castlingRights(CastlingRights castlingRights) {
    return castlingRightsNumbers[castlingRights];
}

uint64_t Zobrist::enPassantSquare(Square square) {
    // Avoiding using branches here.
    //
    // (square >> 6) tells us if square is valid or not. (valid if 0, invalid if 1)
    // The file of an invalid square is 0.
    //
    // If square is invalid:        square = 64, so (64.file() + 1) - (64 >> 6) = 1 - 1 = 0 (index of no en passant)
    // If square is valid e.g. a6:  square = 40, so (40.file() + 1) - (40 >> 6) = 1 - 0 = 1 (index of A file en passant)
    uint8_t index = (square.file() + 1) - (square >> 6);
    return enPassantFileNumbers[index];
}

uint64_t Zobrist::piece(Piece piece, Square square) {
    return pieceNumbers[piece.color()][piece.type()][square];
}



TranspositionTable::LockedEntry::LockedEntry() {
    this->entry_ = nullptr;
}

TranspositionTable::LockedEntry::LockedEntry(Entry *entry) {
    entry->lock().lock();
    this->entry_ = entry;
}

TranspositionTable::LockedEntry::~LockedEntry() {
    if (this->entry_ != nullptr) {
        this->entry_->lock().unlock();
    }
}

void TranspositionTable::Entry::store(uint64_t key, uint16_t depth, Flag flag, Move bestMove, int32_t bestScore) {
    this->key_ = key;
    this->depth_ = depth;
    this->flag_ = flag;
    this->bestMove_ = bestMove;
    this->bestScore_ = bestScore;
}

TranspositionTable::TranspositionTable(uint32_t size) {
    bool powerOfTwo = (size != 0) && !(size & (size - 1));
    assert(powerOfTwo && "Transposition table size must be a power of two.");

    this->sizeMask_ = size - 1;

    this->entries_ = static_cast<Entry *>(std::calloc(size, sizeof(Entry)));
}

TranspositionTable::~TranspositionTable() {
    std::free(this->entries_);
}

TranspositionTable::LockedEntry TranspositionTable::load(uint64_t key) const {
    LockedEntry lockedEntry(this->entries_ + (key & this->sizeMask_));

    if (lockedEntry->isValid() && lockedEntry->key() == key) {
        return lockedEntry;
    } else {
        return { };
    }
}

void TranspositionTable::store(uint64_t key, uint16_t depth, Flag flag, Move bestMove, int32_t bestScore) {
    LockedEntry lockedEntry(this->entries_ + (key & this->sizeMask_));

    // Only overwrite an existing entry if the new entry has a higher depth
    if (!lockedEntry->isValid() || depth > lockedEntry->depth()) {
        // Write the new entry
        lockedEntry->store(key, depth, flag, bestMove, bestScore);
    }
}

TranspositionTable::Entry *TranspositionTable::debugLoadWithoutLock(uint64_t key) {
    return this->entries_ + (key & this->sizeMask_);
}

void TranspositionTable::debugStoreWithoutLock(uint64_t key, uint16_t depth, Flag flag, Move bestMove,
    int32_t bestScore) {
    Entry *entry = this->entries_ + (key & this->sizeMask_);
    entry->store(key, depth, flag, bestMove, bestScore);
}
