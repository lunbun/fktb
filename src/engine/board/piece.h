#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <cassert>

#include "color.h"
#include "engine/inline.h"

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
}
using PieceType = PieceTypeNamespace::PieceType;

namespace PieceMaterial {
    constexpr int32_t Pawn          = 100;
    constexpr int32_t Knight        = 320;
    constexpr int32_t Bishop        = 330;
    constexpr int32_t Rook          = 500;
    constexpr int32_t Queen         = 900;
    constexpr int32_t King          = 0;
    constexpr std::array<int32_t, 6> Values = { Pawn, Knight, Bishop, Rook, Queen, King };

    constexpr int32_t BishopPair    = 50;

    INLINE constexpr int32_t material(PieceType pieceType) { return PieceMaterial::Values[pieceType]; }
}
// @formatter:on



struct Piece {
    INLINE constexpr static Piece empty() { return { PieceType::Empty, Color::White }; }
    INLINE constexpr static Piece pawn(Color color) { return { PieceType::Pawn, color }; }
    INLINE constexpr static Piece knight(Color color) { return { PieceType::Knight, color }; }
    INLINE constexpr static Piece bishop(Color color) { return { PieceType::Bishop, color }; }
    INLINE constexpr static Piece rook(Color color) { return { PieceType::Rook, color }; }
    INLINE constexpr static Piece queen(Color color) { return { PieceType::Queen, color }; }
    INLINE constexpr static Piece king(Color color) { return { PieceType::King, color }; }

    INLINE constexpr static Piece white(PieceType type) { return { type, Color::White }; }
    INLINE constexpr static Piece black(PieceType type) { return { type, Color::Black }; }

    INLINE constexpr Piece() : Piece(PieceType::Empty, Color::White) { }
    INLINE constexpr Piece(PieceType type, Color color) : bits_((color << 3) | type) { }

    [[nodiscard]] INLINE constexpr Color color() const { return static_cast<Color>(this->bits_ >> 3); }
    [[nodiscard]] INLINE constexpr PieceType type() const { return static_cast<PieceType>(this->bits_ & 7); }
    [[nodiscard]] INLINE constexpr bool isEmpty() const { return this->type() == PieceType::Empty; }

    [[nodiscard]] INLINE constexpr int32_t material() const { return PieceMaterial::material(this->type()); }

    [[nodiscard]] INLINE constexpr bool operator==(Piece other) const { return this->bits_ == other.bits_; }

    [[nodiscard]] std::string debugName() const;

private:
    uint8_t bits_;
};

static_assert(sizeof(Piece) == 1, "Piece must be 1 byte");
