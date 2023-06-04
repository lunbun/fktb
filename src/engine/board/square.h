#pragma once

#include <cstdint>
#include <string>
#include <array>

#include "engine/inline.h"

// Square can technically fit into 6 bits.
class Square {
public:
    // @formatter:off
    constexpr static uint8_t A1 = 0,  B1 = 1,  C1 = 2,  D1 = 3,  E1 = 4,  F1 = 5,  G1 = 6,  H1 = 7;
    constexpr static uint8_t A2 = 8,  B2 = 9,  C2 = 10, D2 = 11, E2 = 12, F2 = 13, G2 = 14, H2 = 15;
    constexpr static uint8_t A3 = 16, B3 = 17, C3 = 18, D3 = 19, E3 = 20, F3 = 21, G3 = 22, H3 = 23;
    constexpr static uint8_t A4 = 24, B4 = 25, C4 = 26, D4 = 27, E4 = 28, F4 = 29, G4 = 30, H4 = 31;
    constexpr static uint8_t A5 = 32, B5 = 33, C5 = 34, D5 = 35, E5 = 36, F5 = 37, G5 = 38, H5 = 39;
    constexpr static uint8_t A6 = 40, B6 = 41, C6 = 42, D6 = 43, E6 = 44, F6 = 45, G6 = 46, H6 = 47;
    constexpr static uint8_t A7 = 48, B7 = 49, C7 = 50, D7 = 51, E7 = 52, F7 = 53, G7 = 54, H7 = 55;
    constexpr static uint8_t A8 = 56, B8 = 57, C8 = 58, D8 = 59, E8 = 60, F8 = 61, G8 = 62, H8 = 63;
    constexpr static uint8_t Invalid = 64;
    // @formatter:on

    INLINE constexpr Square() : index_(0) { }
    INLINE constexpr Square(uint8_t index) : index_(index) { } // NOLINT(google-explicit-constructor)
    INLINE constexpr Square(uint8_t file, uint8_t rank) : index_(file | (rank << 3)) { }
    INLINE constexpr operator uint8_t() const { return this->index_; } // NOLINT(google-explicit-constructor)

    [[nodiscard]] INLINE constexpr static uint8_t file(Square square) { return square & 7; }
    [[nodiscard]] INLINE constexpr static uint8_t rank(Square square) { return square >> 3; };

    [[nodiscard]] INLINE constexpr uint8_t file() const { return Square::file(this->index_); }
    [[nodiscard]] INLINE constexpr uint8_t rank() const { return Square::rank(this->index_); }

    [[nodiscard]] std::string uci() const;
    [[nodiscard]] std::string debugName() const;

private:
    uint8_t index_;
};

static_assert(sizeof(Square) == 1, "Square must be 1 byte");

template<typename T>
using SquareMap = std::array<T, 64>;
