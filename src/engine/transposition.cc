#include "transposition.h"

#include <cstdlib>
#include <cstdint>
#include <random>
#include <array>
#include <stdexcept>

#include "piece.h"
#include "move.h"

namespace ZobristHashNumbers {
    const uint64_t seed = 0;

    bool isInitialized = false;

    uint64_t blackToMoveNumber;
    ColorMap<std::array<std::array<uint64_t, 64>, 6>> pieceNumbers;

    // Initializes the random numbers used for the Zobrist hash, if not already initialized.
    void maybeInitialize() {
        if (isInitialized) {
            return;
        }

        isInitialized = true;

        std::mt19937_64 generator(seed);

        blackToMoveNumber = generator();
        for (uint8_t pieceType = 0; pieceType < 6; ++pieceType) {
            for (uint8_t square = 0; square < 64; ++square) {
                pieceNumbers.white()[pieceType][square] = generator();
                pieceNumbers.black()[pieceType][square] = generator();
            }
        }
    }

    uint64_t getPieceNumber(PieceType type, PieceColor color, Square square) {
        return pieceNumbers[color][static_cast<uint8_t>(type)][square.index()];
    }

    uint64_t getPieceNumber(Piece piece) {
        return getPieceNumber(piece.type(), piece.color(), piece.square());
    }
}



ZobristHash::ZobristHash(PieceColor turn) : hash_(0) {
    ZobristHashNumbers::maybeInitialize();

    if (turn == PieceColor::Black) {
        this->hash_ ^= ZobristHashNumbers::blackToMoveNumber;
    }
}

void ZobristHash::piece(Piece piece) {
    this->hash_ ^= ZobristHashNumbers::getPieceNumber(piece);
}

void ZobristHash::move(Move move) {
    // Remove the piece from its old square
    this->hash_ ^= ZobristHashNumbers::getPieceNumber(move.piece());

    // Add the piece to its new square
    this->hash_ ^= ZobristHashNumbers::getPieceNumber(move.pieceType(), move.pieceColor(), move.to());

    if (move.isCapture()) {
        // Remove the captured piece from its square
        this->hash_ ^= ZobristHashNumbers::getPieceNumber(move.capturedPiece());
    }

    // Switch the turn
    this->hash_ ^= ZobristHashNumbers::blackToMoveNumber;
}



TranspositionTable::Entry::Entry(const ZobristHash &hash, int32_t depth, Flag flag, std::optional<Move> bestMove,
    int32_t bestScore) : isValid(true), key(hash.hash()), depth(depth), flag(flag), bestMove(bestMove),
                         bestScore(bestScore) { }

TranspositionTable::TranspositionTable(uint32_t size) : lock_() {
    bool powerOfTwo = (size != 0) && !(size & (size - 1));
    if (!powerOfTwo) {
        throw std::invalid_argument("size must be a power of two");
    }

    this->sizeMask_ = size - 1;

    this->entries_ = static_cast<Entry *>(calloc(size, sizeof(Entry)));
}

TranspositionTable::~TranspositionTable() {
    free(this->entries_);
}

TranspositionTable::Entry *TranspositionTable::load(const ZobristHash &hash) const {
    uint64_t key = hash.hash();

    Entry *entry = this->entries_ + (key & this->sizeMask_);

    if (entry->isValid && entry->key == key) {
        return entry;
    } else {
        return nullptr;
    }
}

void TranspositionTable::store(const ZobristHash &hash, int32_t depth, Flag flag, std::optional<Move> bestMove,
    int32_t bestScore) {
    uint64_t key = hash.hash();

    Entry *pointer = this->entries_ + (key & this->sizeMask_);

    // Only overwrite an existing entry if the new entry has a higher depth
    if (!pointer->isValid || depth > pointer->depth) {
        // Emplace the new entry
        new(pointer) Entry(hash, depth, flag, bestMove, bestScore);
    }
}
