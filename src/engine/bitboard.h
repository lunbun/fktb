#pragma once

#include <cstdint>
#include <cassert>
#include <string>
#include <array>

#include "piece.h"
#include "inline.h"

INLINE uint8_t bitScanForward(uint64_t x) {
    assert(x != 0);
    asm ("bsfq %0, %0" : "=r" (x) : "0" (x));
    return (int) x;
}

INLINE uint8_t bitScanReverse(uint64_t x) {
    assert(x != 0);
    asm ("bsrq %0, %0" : "=r" (x) : "0" (x));
    return (int) x;
}

INLINE uint8_t popCount(uint64_t x) {
    asm ("popcntq %0, %0" : "=r" (x) : "0" (x));
    return (int) x;
}

INLINE uint64_t byteSwap(uint64_t x) {
    asm ("bswapq %0" : "=r" (x) : "0" (x));
    return x;
}

class Bitboard {
public:
    INLINE constexpr Bitboard() : set_(0) { }
    INLINE constexpr Bitboard(uint64_t set) : set_(set) { } // NOLINT(google-explicit-constructor)
    INLINE constexpr operator uint64_t() const { return this->set_; } // NOLINT(google-explicit-constructor)

    // Returns the index of the least significant bit.
    [[nodiscard]] INLINE uint8_t bsf() const { return bitScanForward(this->set_); }

    // Returns the index of the least significant bit and clears it.
    [[nodiscard]] INLINE uint8_t bsfReset() {
        uint8_t index = bitScanForward(this->set_);
        this->set_ &= this->set_ - 1;
        return index;
    }

    // Returns the index of the most significant bit.
    [[nodiscard]] INLINE uint8_t bsr() const { return bitScanReverse(this->set_); }

    // Returns the number of bits in the bitboard.
    [[nodiscard]] INLINE uint8_t count() const { return popCount(this->set_); }

    [[nodiscard]] INLINE bool get(uint8_t index) const { return (this->set_ & (1ULL << index)) != 0; }
    INLINE void set(uint8_t index) { this->set_ |= (1ULL << index); }
    INLINE void clear(uint8_t index) { this->set_ &= ~(1ULL << index); }

    // Flips the board vertically.
    [[nodiscard]] INLINE Bitboard flipVertical() const { return byteSwap(this->set_); }

    [[nodiscard]] std::string debug() const;

private:
    uint64_t set_;
};



using AttackTable = std::array<Bitboard, 64>;

namespace Bitboards {
    constexpr Bitboard Rank1 = 0x00000000000000FFULL;
    constexpr Bitboard Rank2 = 0x000000000000FF00ULL;
    constexpr Bitboard Rank3 = 0x0000000000FF0000ULL;
    constexpr Bitboard Rank4 = 0x00000000FF000000ULL;
    constexpr Bitboard Rank5 = 0x000000FF00000000ULL;
    constexpr Bitboard Rank6 = 0x0000FF0000000000ULL;
    constexpr Bitboard Rank7 = 0x00FF000000000000ULL;
    constexpr Bitboard Rank8 = 0xFF00000000000000ULL;

    // Ray attack tables.
    extern AttackTable northAttacks;
    extern AttackTable northEastAttacks;
    extern AttackTable eastAttacks;
    extern AttackTable southEastAttacks;
    extern AttackTable southAttacks;
    extern AttackTable southWestAttacks;
    extern AttackTable westAttacks;
    extern AttackTable northWestAttacks;

    // Offset attack tables.
    extern ColorMap<AttackTable> pawnAttacks;
    extern AttackTable knightAttacks;
    extern AttackTable kingAttacks;

    // Initialize the bitboard attack tables, if they haven't been initialized already.
    void maybeInit();

    Bitboard orthogonal(Square square, Bitboard occupied);
    Bitboard diagonal(Square square, Bitboard occupied);

    template<Color Side>
    INLINE Bitboard pawn(Square square) {
        return pawnAttacks[Side][square];
    }
    INLINE Bitboard knight(Square square) {
        return knightAttacks[square];
    }
    INLINE Bitboard bishop(Square square, Bitboard occupied) {
        return diagonal(square, occupied);
    }
    INLINE Bitboard rook(Square square, Bitboard occupied) {
        return orthogonal(square, occupied);
    }
    INLINE Bitboard queen(Square square, Bitboard occupied) {
        return orthogonal(square, occupied) | diagonal(square, occupied);
    }
    INLINE Bitboard king(Square square) {
        return kingAttacks[square];
    }

    // Returns a bitboard with all attacks of the given piece type.
    template<Color Side>
    Bitboard allPawn(Bitboard pawns);
    Bitboard allKnight(Bitboard knights);
    Bitboard allBishop(Bitboard bishops, Bitboard occupied);
    Bitboard allRook(Bitboard rooks, Bitboard occupied);
    Bitboard allQueen(Bitboard queens, Bitboard occupied);
    Bitboard allKing(Bitboard kings);
}
