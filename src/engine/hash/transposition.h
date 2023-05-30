#pragma once

#include <cstdint>
#include <atomic>

#include "lock.h"
#include "engine/inline.h"
#include "engine/move/move.h"
#include "engine/board/square.h"
#include "engine/board/castling.h"
#include "engine/board/piece.h"

namespace Zobrist {
    // Initializes the Zobrist hash numbers, if not already initialized.
    void maybeInit();

    uint64_t blackToMove();
    uint64_t castlingRights(CastlingRights castlingRights);
    uint64_t piece(Piece piece, Square square);
}



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
        Entry() = delete;
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

        [[nodiscard]] INLINE SpinLock &lock() { return this->lock_; }

        // It is the caller's responsibility to lock the entry before calling this method.
        void store(uint64_t key, uint16_t depth, Flag flag, Move bestMove, int32_t bestScore);

    private:
        // Fields are arranged from largest to smallest to minimize padding.
        uint64_t key_;
        int32_t bestScore_;
        uint16_t depth_;
        Move bestMove_;
        Flag flag_;
        SpinLock lock_;
    };

    // RAII wrapper for locking an entry.
    class LockedEntry {
    public:
        LockedEntry();
        explicit LockedEntry(Entry *entry);
        ~LockedEntry();

        [[nodiscard]] INLINE constexpr Entry &operator*() const { return *this->entry_; }
        [[nodiscard]] INLINE constexpr Entry *operator->() const { return this->entry_; }

        [[nodiscard]] INLINE constexpr bool operator==(std::nullptr_t) const { return this->entry_ == nullptr; }
        [[nodiscard]] INLINE constexpr bool operator!=(std::nullptr_t) const { return this->entry_ != nullptr; }

    private:
        Entry *entry_;
    };

    static_assert(sizeof(Entry) == 24, "TranspositionTable::Entry must be 24 bytes.");

    explicit TranspositionTable(uint32_t size);
    ~TranspositionTable();

    TranspositionTable(const TranspositionTable &other) = delete;
    TranspositionTable &operator=(const TranspositionTable &other) = delete;
    TranspositionTable(TranspositionTable &&other) = delete;
    TranspositionTable &operator=(TranspositionTable &&other) = delete;

    // Will only return a valid entry, nullptr otherwise.
    // Will lock the entry for you.
    [[nodiscard]] LockedEntry load(uint64_t key) const;

    // Will lock the entry for you.
    void store(uint64_t key, uint16_t depth, Flag flag, Move bestMove, int32_t bestScore);

    // USE ONLY FOR TESTING! These methods do not lock the entry, and do not check if the keys match. These methods
    // will cause table corruption if used incorrectly.
    Entry *debugLoadWithoutLock(uint64_t key);
    void debugStoreWithoutLock(uint64_t key, uint16_t depth, Flag flag, Move bestMove, int32_t bestScore);

private:
    uint32_t sizeMask_;
    Entry *entries_;
};
