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
    // @formatter:on

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

    // Using PEXT bitboards for generating sliding piece attacks: https://www.chessprogramming.org/BMI2#PEXTBitboards
    // (similar to magic bitboards, but without the magic :) )
    extern SquareMap<Bitboard> diagonalMasks;
    extern SquareMap<Bitboard> orthogonalMasks;
    // TODO: Sliding piece attack tables can be smaller.
    extern SquareMap<std::array<Bitboard, 512>> diagonalAttacks;    // 256 KiB
    extern SquareMap<std::array<Bitboard, 4096>> orthogonalAttacks; // 2 MiB

    extern ColorMap<SquareMap<Bitboard>> pawnAttacks;
    extern SquareMap<Bitboard> knightAttacks;
    extern SquareMap<Bitboard> kingAttacks;

    // Initialize the bitboard attack tables, if they haven't been initialized already.
    void maybeInit();

    template<Color Side>
    INLINE Bitboard pawn(Square square) {
        return pawnAttacks[Side][square];
    }

    INLINE Bitboard knight(Square square) {
        return knightAttacks[square];
    }

    INLINE Bitboard bishop(Square square, Bitboard occupied) {
        uint64_t index = Intrinsics::pext(occupied, diagonalMasks[square]);

        assert(index < 512);
        return diagonalAttacks[square][index];
    }

    INLINE Bitboard rook(Square square, Bitboard occupied) {
        uint64_t index = Intrinsics::pext(occupied, orthogonalMasks[square]);

        assert(index < 4096);
        return orthogonalAttacks[square][index];
    }

    INLINE Bitboard queen(Square square, Bitboard occupied) {
        return bishop(square, occupied) | rook(square, occupied);
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
}
