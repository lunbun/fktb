#pragma once

#include <cstdint>
#include <cassert>
#include <string>
#include <array>
#include <iterator>

#include "color.h"
#include "square.h"
#include "piece.h"
#include "engine/inline.h"
#include "engine/intrinsics.h"

class Bitboard {
public:
    INLINE constexpr Bitboard() : set_(0) { }
    INLINE constexpr Bitboard(uint64_t set) : set_(set) { } // NOLINT(google-explicit-constructor)
    INLINE constexpr operator uint64_t() const { return this->set_; } // NOLINT(google-explicit-constructor)

    // Returns the number of bits in the bitboard.
    [[nodiscard]] INLINE uint8_t count() const { return Intrinsics::popcnt(this->set_); }

    [[nodiscard]] INLINE constexpr bool get(uint8_t index) const { return (this->set_ & (1ULL << index)) != 0; }
    INLINE void set(uint8_t index) { this->set_ |= (1ULL << index); }
    INLINE void clear(uint8_t index) { this->set_ &= ~(1ULL << index); }

    // @formatter:off
    INLINE Bitboard &operator|=(Bitboard other) { this->set_ |= other.set_; return *this; }
    INLINE Bitboard &operator&=(Bitboard other) { this->set_ &= other.set_; return *this; }
    INLINE Bitboard &operator^=(Bitboard other) { this->set_ ^= other.set_; return *this; }
    INLINE Bitboard &operator<<=(uint8_t shift) { this->set_ <<= shift; return *this; }
    INLINE Bitboard &operator>>=(uint8_t shift) { this->set_ >>= shift; return *this; }
    // @formatter:on

    // Shifts the bitboard forward/backward by the given number of ranks. Forward is towards the 8th rank for white and towards
    // the 1st rank for black.
    template<Color Side>
    [[nodiscard]] INLINE constexpr Bitboard shiftForward(uint8_t ranks) const;
    template<Color Side>
    [[nodiscard]] INLINE constexpr Bitboard shiftBackward(uint8_t ranks) const;

    // Iterates over all the bits in the bitboard using a C++ iterator.
    //
    // Tested in godbolt, when compiled with -O3, the iterator produces the exact same assembly as a while loop like this:
    // while (bitboard) {
    //     uint8_t index = bitboard.bsf();
    //     // Do something with index.
    //     bitboard &= bitboard - 1;
    // }
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using difference_type = uint8_t;
        using value_type = uint8_t;
        using pointer = uint8_t *;
        using reference = uint8_t &;

        INLINE constexpr Iterator() : set_(0) { }
        INLINE constexpr Iterator(uint64_t set) : set_(set) { } // NOLINT(google-explicit-constructor)

        INLINE constexpr bool operator==(const Iterator &other) const { return this->set_ == other.set_; }
        INLINE constexpr bool operator!=(const Iterator &other) const { return this->set_ != other.set_; }

        INLINE uint8_t operator*() const { return Intrinsics::bsf(this->set_); }

        INLINE Iterator &operator++() {
            this->set_ = Intrinsics::blsr(this->set_);
            return *this;
        }

    private:
        uint64_t set_;
    };
    [[nodiscard]] INLINE constexpr Iterator begin() const { return this->set_; }
    INLINE constexpr static Iterator end() { return 0; }

    [[nodiscard]] std::string debug() const;

private:
    uint64_t set_;
};

template<Color Side>
INLINE constexpr Bitboard Bitboard::shiftForward(uint8_t ranks) const {
    if constexpr (Side == Color::White) {
        return this->set_ << (ranks * 8);
    } else {
        return this->set_ >> (ranks * 8);
    }
}

template<Color Side>
INLINE constexpr Bitboard Bitboard::shiftBackward(uint8_t ranks) const {
    // Shifting backward is equivalent to shifting forward in the opposite direction.
    return this->shiftForward<~Side>(ranks);
}



class Board;

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

#define BB_FILE(file) constexpr Bitboard File ## file = 0x0101010101010101ULL << Square:: file ## 1;
    BB_FILE(A) BB_FILE(B) BB_FILE(C) BB_FILE(D) BB_FILE(E) BB_FILE(F) BB_FILE(G) BB_FILE(H)
#undef BB_FILE

#define BB_RANK(rank) constexpr Bitboard Rank ## rank = 0xFFULL << (8 * (rank - 1));
    BB_RANK(1) BB_RANK(2) BB_RANK(3) BB_RANK(4) BB_RANK(5) BB_RANK(6) BB_RANK(7) BB_RANK(8)
#undef BB_RANK

    constexpr Bitboard Empty = 0ULL;
    constexpr Bitboard All = 0xFFFFFFFFFFFFFFFFULL;

    INLINE constexpr Bitboard file(uint8_t file) { return FileA << file; }
    INLINE constexpr Bitboard rank(uint8_t rank) { return Rank1 << (8 * (rank - 1)); }
    // @formatter:on

    // Initialize the bitboard attack tables, if they haven't been initialized already.
    void maybeInit();

    // Returns a bitboard with all the squares between the two given squares, exclusive.
    Bitboard between(Square a, Square b);

    template<Color Side>
    Bitboard pawnAttacks(Square square);
    Bitboard knightAttacks(Square square);
    Bitboard bishopAttacks(Square square, Bitboard occupied);
    Bitboard rookAttacks(Square square, Bitboard occupied);
    Bitboard queenAttacks(Square square, Bitboard occupied);
    Bitboard kingAttacks(Square square);

    // Slider attacks on an empty board.
    Bitboard rookAttacksOnEmpty(Square square);
    Bitboard bishopAttacksOnEmpty(Square square);
    Bitboard queenAttacksOnEmpty(Square square);

    // Returns a bitboard with all attacks of the given piece type.
    template<Color Side>
    Bitboard allPawnAttacks(Bitboard pawns);
    Bitboard allKnightAttacks(Bitboard knights);
    Bitboard allBishopAttacks(Bitboard bishops, Bitboard occupied);
    Bitboard allRookAttacks(Bitboard rooks, Bitboard occupied);
    Bitboard allQueenAttacks(Bitboard queens, Bitboard occupied);

    // Returns a bitboard with all attacks of the given side.
    template<Color Side>
    Bitboard allAttacks(const Board &board, Bitboard occupied);
}
