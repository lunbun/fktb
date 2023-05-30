#pragma once

#include <cstdint>
#include <cassert>
#include <string>
#include <array>

#include "color.h"
#include "square.h"
#include "piece.h"
#include "engine/inline.h"

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
    // Bitboards for all the squares, files, and ranks.
    // @formatter:off
#define BB_SQ(square) constexpr Bitboard square = 1ULL << (Square::square);
    BB_SQ(A1) BB_SQ(B1) BB_SQ(C1) BB_SQ(D1) BB_SQ(E1) BB_SQ(F1) BB_SQ(G1) BB_SQ(H1)
    BB_SQ(A2) BB_SQ(B2) BB_SQ(C2) BB_SQ(D2) BB_SQ(E2) BB_SQ(F2) BB_SQ(G2) BB_SQ(H2)
    BB_SQ(A3) BB_SQ(B3) BB_SQ(C3) BB_SQ(D3) BB_SQ(E3) BB_SQ(F3) BB_SQ(G3) BB_SQ(H3)
    BB_SQ(A4) BB_SQ(B4) BB_SQ(C4) BB_SQ(D4) BB_SQ(E4) BB_SQ(F4) BB_SQ(G4) BB_SQ(H4)
    BB_SQ(A5) BB_SQ(B5) BB_SQ(C5) BB_SQ(D5) BB_SQ(E5) BB_SQ(F5) BB_SQ(G5) BB_SQ(H5)
    BB_SQ(A6) BB_SQ(B6) BB_SQ(C6) BB_SQ(D6) BB_SQ(E6) BB_SQ(F6) BB_SQ(G6) BB_SQ(H6)
    BB_SQ(A7) BB_SQ(B7) BB_SQ(C7) BB_SQ(D7) BB_SQ(E7) BB_SQ(F7) BB_SQ(G7) BB_SQ(H7)
    BB_SQ(A8) BB_SQ(B8) BB_SQ(C8) BB_SQ(D8) BB_SQ(E8) BB_SQ(F8) BB_SQ(G8) BB_SQ(H8)
#undef BB_SQ

#define BB_FILE(file) constexpr Bitboard File ## file = 0x0101010101010101ULL << (file - 1);
    BB_FILE(1) BB_FILE(2) BB_FILE(3) BB_FILE(4) BB_FILE(5) BB_FILE(6) BB_FILE(7) BB_FILE(8)
#undef BB_FILE

#define BB_RANK(rank) constexpr Bitboard Rank ## rank = 0xFFULL << (8 * (rank - 1));
    BB_RANK(1) BB_RANK(2) BB_RANK(3) BB_RANK(4) BB_RANK(5) BB_RANK(6) BB_RANK(7) BB_RANK(8)
#undef BB_RANK
    // @formatter:on

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
