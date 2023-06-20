#pragma once

#include <cstdint>
#include <atomic>

#include "engine/inline.h"
#include "engine/move/move.h"
#include "engine/board/square.h"
#include "engine/board/castling.h"
#include "engine/board/piece.h"

namespace Zobrist {
    // Initializes the Zobrist hash numbers, if not already initialized.
    void init();

    uint64_t blackToMove();
    uint64_t castlingRights(CastlingRights castlingRights);
    uint64_t enPassantSquare(Square square);
    uint64_t piece(Piece piece, Square square);
}



class TranspositionTable {
public:
    constexpr static uint32_t MinimumTableSizeLog2 = 20;
    constexpr static uint32_t MinimumTableSize = 1 << MinimumTableSizeLog2;

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

        [[nodiscard]] INLINE constexpr bool isValid() const { return this->flag() != Flag::Invalid; }
        [[nodiscard]] INLINE constexpr uint64_t upperBitsKey() const;
        [[nodiscard]] INLINE constexpr uint16_t depth() const;
        [[nodiscard]] INLINE constexpr Flag flag() const;
        [[nodiscard]] INLINE constexpr Move bestMove() const;
        [[nodiscard]] INLINE constexpr int32_t bestScore() const;

        void store(uint64_t key, uint16_t depth, Flag flag, Move bestMove, int32_t bestScore);

    private:
        // Assuming MinimumTableSizeLog2 is 20, the key only needs to be the upper 44 bits, since the lower 20 bits are the index.
        //
        //                  Field        Bits            Size
        //          [---    key          0 - 43          44 bits
        // data1_   |       depth        44 - 59         16 bits
        //          |       flag         60 - 61         2 bits
        //          [---    padding      62 - 63         2 bits
        //
        //          [--     bestMove     0 - 15          16 bits
        // data2_   |       bestScore    16 - 47         32 bits
        //          [---    padding      48 - 63         16 bits
        //
        uint64_t data1_, data2_;
    };

    static_assert(sizeof(Entry) == 16, "TranspositionTable::Entry must be 16 bytes.");

    explicit TranspositionTable(uint32_t size);
    ~TranspositionTable();

    TranspositionTable(const TranspositionTable &other) = delete;
    TranspositionTable &operator=(const TranspositionTable &other) = delete;
    TranspositionTable(TranspositionTable &&other) = delete;
    TranspositionTable &operator=(TranspositionTable &&other) = delete;

    void clear();

    // Returns nullptr if the entry is invalid or the key does not match.
    [[nodiscard]] Entry *load(uint64_t key) const;

    // Stores the entry if there was no entry previously, or if the new entry is "higher quality" than the previous entry (e.g.
    // does the new entry have a higher depth?).
    void maybeStore(uint64_t key, uint16_t depth, Flag flag, Move bestMove, int32_t bestScore);

private:
    // Data 1 fields:
    constexpr static uint64_t UpperBitsKeySize = 64 - MinimumTableSizeLog2;
    constexpr static uint64_t UpperBitsKeyMask = (1ULL << UpperBitsKeySize) - 1;
    constexpr static uint64_t UpperBitsKeyShift = 0;

    constexpr static uint64_t DepthSize = 16;
    constexpr static uint64_t DepthMask = (1ULL << DepthSize) - 1;
    constexpr static uint64_t DepthShift = UpperBitsKeyShift + UpperBitsKeySize;

    constexpr static uint64_t FlagSize = 2;
    constexpr static uint64_t FlagMask = (1ULL << FlagSize) - 1;
    constexpr static uint64_t FlagShift = DepthShift + DepthSize;

    // Data 2 fields:
    constexpr static uint64_t BestMoveSize = 16;
    constexpr static uint64_t BestMoveMask = (1ULL << BestMoveSize) - 1;
    constexpr static uint64_t BestMoveShift = 0;

    constexpr static uint64_t BestScoreSize = 32;
    constexpr static uint64_t BestScoreMask = (1ULL << BestScoreSize) - 1;
    constexpr static uint64_t BestScoreShift = BestMoveShift + BestMoveSize;


    uint32_t size_;
    uint32_t indexMask_;
    Entry *entries_;
};

INLINE constexpr uint64_t TranspositionTable::Entry::upperBitsKey() const {
    return (this->data1_ >> UpperBitsKeyShift) & UpperBitsKeyMask;
}

INLINE constexpr uint16_t TranspositionTable::Entry::depth() const {
    return static_cast<uint16_t>((this->data1_ >> DepthShift) & DepthMask);
}

INLINE constexpr TranspositionTable::Flag TranspositionTable::Entry::flag() const {
    return static_cast<Flag>((this->data1_ >> FlagShift) & FlagMask);
}

INLINE constexpr Move TranspositionTable::Entry::bestMove() const {
    return Move(static_cast<uint16_t>((this->data2_ >> BestMoveShift) & BestMoveMask));
}

INLINE constexpr int32_t TranspositionTable::Entry::bestScore() const {
    return static_cast<int32_t>((this->data2_ >> BestScoreShift) & BestScoreMask);
}
