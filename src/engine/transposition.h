#pragma once

#include <cstdint>
#include <optional>

#include "move.h"
#include "piece.h"

class ZobristHash {
public:
    explicit ZobristHash(PieceColor turn);

    [[nodiscard]] inline uint64_t hash() const { return this->hash_; }

    // Adds or removes a piece from the hash. Both adding and removing are done with the same function since xor is its
    // own inverse.
    void piece(Piece piece);

    // Makes or unmakes a move, and switches the turn. Since xor is its own inverse, the same function can be used for
    // both make and unmake moves.
    void move(Move move);

private:
    uint64_t hash_;
};

class TranspositionTable {
public:
    enum class Flag {
        Exact,
        LowerBound,
        UpperBound,
    };

    struct Entry {
        bool isValid;
        uint64_t key;
        int32_t depth;
        Flag flag;

        std::optional<Move> bestMove;
        int32_t bestScore;

        Entry() = delete;
    };

    explicit TranspositionTable(uint32_t size);
    ~TranspositionTable();

    // Will only return a valid entry, nullptr otherwise.
    [[nodiscard]] Entry *load(const ZobristHash &hash) const;

    void store(const ZobristHash &hash, Entry entry);

private:
    uint32_t sizeMask_;
    Entry *entries_;
};
