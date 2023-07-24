#pragma once

#include <cstdint>
#include <string>

#include "color.h"
#include "piece.h"
#include "square.h"

namespace FKTB {

// @formatter:off
namespace CastlingSideNamespace {
enum CastlingSide : uint8_t {
    King    = 0b0001,
    Queen   = 0b0010
};
} // namespace CastlingSideNamespace
using CastlingSide = CastlingSideNamespace::CastlingSide;
// @formatter:on

namespace CastlingRook {

// Returns the square that the rook is on for the given color and side.
[[nodiscard]] Square from(Color color, CastlingSide side);

// Returns the square that the rook will be on for the given color and side.
[[nodiscard]] Square to(Color color, CastlingSide side);

} // namespace CastlingRook

class CastlingRights {
public:
    // @formatter:off
    constexpr static uint8_t WhiteKingSide  = CastlingSide::King    << 0;   // 0b0001
    constexpr static uint8_t WhiteQueenSide = CastlingSide::Queen   << 0;   // 0b0010
    constexpr static uint8_t BlackKingSide  = CastlingSide::King    << 2;   // 0b0100
    constexpr static uint8_t BlackQueenSide = CastlingSide::Queen   << 2;   // 0b1000
    constexpr static uint8_t None           = 0b0000;
    constexpr static uint8_t All            = 0b1111;
    // @formatter:on

    [[nodiscard]] INLINE constexpr static CastlingRights color(Color color) {
        return 0b0011 << (color * 2);
    }
    // Returns the castling rights for the given rook square. Assumes that the square is a valid rook square, will
    // return garbage results otherwise.
    [[nodiscard]] static CastlingRights fromRookSquare(Square square);

    INLINE constexpr CastlingRights() : bits_(CastlingRights::All) { }
    INLINE constexpr CastlingRights(uint8_t bits) : bits_(bits) { } // NOLINT(google-explicit-constructor)
    INLINE constexpr operator uint8_t() const { return this->bits_; } // NOLINT(google-explicit-constructor)

    [[nodiscard]] INLINE constexpr bool has(CastlingRights right) const { return (this->bits_ & right) != 0; }

    [[nodiscard]] INLINE CastlingRights without(CastlingRights rights) const { return this->bits_ & ~rights; }
    [[nodiscard]] INLINE CastlingRights without(Color color) const {
        return this->bits_ & ~CastlingRights::color(color);
    }

    // @formatter:off
    INLINE CastlingRights &operator|=(CastlingRights other) { this->bits_ |= other.bits_; return *this; }
    INLINE CastlingRights &operator&=(CastlingRights other) { this->bits_ &= other.bits_; return *this; }
    INLINE CastlingRights &operator^=(CastlingRights other) { this->bits_ ^= other.bits_; return *this; }
    // @formatter:on

    [[nodiscard]] std::string debugName() const;

private:
    uint8_t bits_;
};

} // namespace FKTB
