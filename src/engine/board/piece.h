#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <cassert>

#include "color.h"
#include "engine/inline.h"

namespace FKTB {

// @formatter:off
namespace PieceTypeNamespace {
enum PieceType : uint8_t {
    Pawn        = 0,
    Knight      = 1,
    Bishop      = 2,
    Rook        = 3,
    Queen       = 4,
    King        = 5,

    Empty       = 7
};

[[nodiscard]] std::string debugName(PieceType pieceType);
} // namespace PieceTypeNamespace
using PieceType = PieceTypeNamespace::PieceType;
// @formatter:on

template<typename T>
class PieceTypeMap {
public:
    constexpr PieceTypeMap() : values_{ } { }
    constexpr PieceTypeMap(const T &pawn, const T &knight, const T &bishop, const T &rook, const T &queen, const T &king)
        : values_{ pawn, knight, bishop, rook, queen, king } { }

    [[nodiscard]] INLINE constexpr T &operator[](PieceType color) { return this->values_[color]; }
    [[nodiscard]] INLINE constexpr const T &operator[](PieceType color) const { return this->values_[color]; }

    [[nodiscard]] INLINE constexpr T &pawn() { return this->values_[0]; }
    [[nodiscard]] INLINE constexpr T &knight() { return this->values_[1]; }
    [[nodiscard]] INLINE constexpr T &bishop() { return this->values_[2]; }
    [[nodiscard]] INLINE constexpr T &rook() { return this->values_[3]; }
    [[nodiscard]] INLINE constexpr T &queen() { return this->values_[4]; }
    [[nodiscard]] INLINE constexpr T &king() { return this->values_[5]; }
    [[nodiscard]] INLINE constexpr const T &pawn() const { return this->values_[0]; };
    [[nodiscard]] INLINE constexpr const T &knight() const { return this->values_[1]; };
    [[nodiscard]] INLINE constexpr const T &bishop() const { return this->values_[2]; };
    [[nodiscard]] INLINE constexpr const T &rook() const { return this->values_[3]; };
    [[nodiscard]] INLINE constexpr const T &queen() const { return this->values_[4]; };
    [[nodiscard]] INLINE constexpr const T &king() const { return this->values_[5]; };

private:
    std::array<T, 6> values_;
};

// @formatter:off
namespace PieceMaterial {
    constexpr int32_t Pawn          = 100;
    constexpr int32_t Knight        = 320;
    constexpr int32_t Bishop        = 330;
    constexpr int32_t Rook          = 500;
    constexpr int32_t Queen         = 900;
    constexpr std::array<int32_t, 5> Values = { Pawn, Knight, Bishop, Rook, Queen };

    constexpr int32_t BishopPair    = 50;

    [[nodiscard]] INLINE constexpr int32_t value(PieceType type);
} // namespace PieceMaterial

INLINE constexpr int32_t PieceMaterial::value(PieceType type) {
    assert(type != PieceType::King);
    assert(type != PieceType::Empty);
    return PieceMaterial::Values[type];
}
// @formatter:on



class Piece {
public:
    INLINE constexpr static Piece empty() { return { Color::White, PieceType::Empty }; }
    INLINE constexpr static Piece pawn(Color color) { return { color, PieceType::Pawn }; }
    INLINE constexpr static Piece knight(Color color) { return { color, PieceType::Knight }; }
    INLINE constexpr static Piece bishop(Color color) { return { color, PieceType::Bishop }; }
    INLINE constexpr static Piece rook(Color color) { return { color, PieceType::Rook }; }
    INLINE constexpr static Piece queen(Color color) { return { color, PieceType::Queen }; }
    INLINE constexpr static Piece king(Color color) { return { color, PieceType::King }; }

    INLINE constexpr static Piece white(PieceType type) { return { Color::White, type }; }
    INLINE constexpr static Piece black(PieceType type) { return { Color::Black, type }; }

    INLINE constexpr Piece() : Piece(Color::White, PieceType::Empty) { }
    INLINE constexpr Piece(Color color, PieceType type) : bits_((color << 3) | type) { }

    [[nodiscard]] INLINE constexpr Color color() const { return static_cast<Color>(this->bits_ >> 3); }
    [[nodiscard]] INLINE constexpr PieceType type() const { return static_cast<PieceType>(this->bits_ & 7); }
    [[nodiscard]] INLINE constexpr bool isEmpty() const { return this->type() == PieceType::Empty; }

    [[nodiscard]] INLINE constexpr int32_t material() const { return PieceMaterial::value(this->type()); }

    [[nodiscard]] INLINE constexpr bool operator==(Piece other) const { return this->bits_ == other.bits_; }

    [[nodiscard]] std::string debugName() const;

private:
    uint8_t bits_;
};

static_assert(sizeof(Piece) == 1, "Piece must be 1 byte");

} // namespace FKTB
