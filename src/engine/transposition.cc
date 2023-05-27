#include "transposition.h"

#include <cstdlib>
#include <cstdint>
#include <random>
#include <array>
#include <cassert>

#include "piece.h"
#include "move.h"

namespace {
    bool isInitialized = false;

    uint64_t blackToMoveNumber = 0;
    ColorMap<std::array<std::array<uint64_t, 64>, 6>> pieceNumbers;
}

void Zobrist::maybeInit() {
    constexpr uint64_t seed = 0;

    if (isInitialized) {
        return;
    }

    isInitialized = true;

    std::mt19937_64 generator(seed);

    blackToMoveNumber = generator();
    for (uint8_t piece = 0; piece < 6; piece++) {
        for (uint8_t square = 0; square < 64; square++) {
            pieceNumbers.white()[piece][square] = generator();
            pieceNumbers.black()[piece][square] = generator();
        }
    }
}

uint64_t Zobrist::blackToMove() {
    return blackToMoveNumber;
}

uint64_t Zobrist::piece(Piece piece, Square square) {
    return pieceNumbers[piece.color()][piece.type()][square];
}



TranspositionTable::TranspositionTable(uint32_t size) : lock_() {
    bool powerOfTwo = (size != 0) && !(size & (size - 1));
    assert(powerOfTwo && "Transposition table size must be a power of two.");

    this->sizeMask_ = size - 1;

    this->entries_ = static_cast<Entry *>(std::calloc(size, sizeof(Entry)));
}

TranspositionTable::~TranspositionTable() {
    std::free(this->entries_);
}

TranspositionTable::Entry *TranspositionTable::load(uint64_t key) const {
    Entry *entry = this->entries_ + (key & this->sizeMask_);

    if (entry->isValid() && entry->key() == key) {
        return entry;
    } else {
        return nullptr;
    }
}

void TranspositionTable::store(uint64_t key, uint16_t depth, Flag flag, Move bestMove, int32_t bestScore) {
    Entry *pointer = this->entries_ + (key & this->sizeMask_);

    // Only overwrite an existing entry if the new entry has a higher depth
    if (!pointer->isValid() || depth > pointer->depth()) {
        // Emplace the new entry
        new(pointer) Entry(key, depth, flag, bestMove, bestScore);
    }
}
