#pragma once

#include <cstdint>

#include "move.h"
#include "piece.h"
#include "lock.h"
#include "inline.h"

namespace Zobrist {
    // Initializes the Zobrist hash numbers, if not already initialized.
    void maybeInit();

    uint64_t blackToMove();
    uint64_t piece(Piece piece, Square square);
}

class ReadonlyTranspositionTable;

class TranspositionTable {
public:
    // @formatter:off
    enum class Flag : uint8_t {
        Invalid         = 0,
        Exact           = 1,
        LowerBound      = 2,
        UpperBound      = 3
    };
    // @formatter:on

    class Entry {
    public:
        INLINE constexpr Entry(uint64_t key, uint16_t depth, Flag flag, Move bestMove, int32_t bestScore)
            : key_(key), bestScore_(bestScore), depth_(depth), bestMove_(bestMove), flag_(flag) { }
        Entry(const Entry &other) = delete;
        Entry &operator=(const Entry &other) = delete;
        Entry(Entry &&other) = delete;
        Entry &operator=(Entry &&other) = delete;

        [[nodiscard]] INLINE constexpr bool isValid() const { return this->flag_ != Flag::Invalid; }
        [[nodiscard]] INLINE constexpr uint64_t key() const { return this->key_; }
        [[nodiscard]] INLINE constexpr uint16_t depth() const { return this->depth_; }
        [[nodiscard]] INLINE constexpr Flag flag() const { return static_cast<Flag>(this->flag_); }
        [[nodiscard]] INLINE constexpr Move bestMove() const { return this->bestMove_; }
        [[nodiscard]] INLINE constexpr int32_t bestScore() const { return this->bestScore_; }

    private:
        // Fields are arranged from largest to smallest to minimize padding.
        uint64_t key_;
        int32_t bestScore_;
        uint16_t depth_;
        Move bestMove_;
        Flag flag_;
    };

    static_assert(sizeof(Entry) == 24, "TranspositionTable::Entry must be 24 bytes.");

    explicit TranspositionTable(uint32_t size);
    ~TranspositionTable();

    // Copy constructor is deleted to prevent accidentally copying the table. Transposition tables are huge, so we
    // really cannot afford to accidentally copy them. Use copy() instead for an explicit copy.
    TranspositionTable(const TranspositionTable &other) = delete;
    TranspositionTable &operator=(const TranspositionTable &other) = delete;
    TranspositionTable(TranspositionTable &&other) noexcept;
    TranspositionTable &operator=(TranspositionTable &&other) noexcept;

    [[nodiscard]] INLINE SpinLock &lock() { return this->lock_; }

    [[nodiscard]] TranspositionTable copy() const;
    [[nodiscard]] ReadonlyTranspositionTable readonlyCopy() const;

    // Will only return a valid entry, nullptr otherwise.
    [[nodiscard]] Entry *load(uint64_t key) const;

    void store(uint64_t key, uint16_t depth, Flag flag, Move bestMove, int32_t bestScore);

private:
    SpinLock lock_;
    uint32_t sizeMask_;
    Entry *entries_;
};

// ReadonlyTranspositionTable is just a wrapper around TranspositionTable since all ReadonlyTranspositionTable does is
// prevent the lock from being used (since no writes will ever occur, there is no need to lock the table).
class ReadonlyTranspositionTable {
public:
    // Will only return a valid entry, nullptr otherwise.
    [[nodiscard]] INLINE TranspositionTable::Entry *load(uint64_t key) const { return this->table_.load(key); }

private:
    friend class TranspositionTable;

    TranspositionTable table_;

    INLINE explicit ReadonlyTranspositionTable(TranspositionTable table) : table_(std::move(table)) { }
};
