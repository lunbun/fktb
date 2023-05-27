#pragma once

#include <cstdint>
#include <string>
#include <array>

#include "inline.h"

// Square can technically fit into 6 bits.
class Square {
public:
    INLINE constexpr Square() : index_(0) { }
    INLINE constexpr Square(uint8_t index) : index_(index) { } // NOLINT(google-explicit-constructor)
    INLINE constexpr Square(uint8_t file, uint8_t rank) : index_(file | (rank << 3)) { }
    INLINE constexpr operator uint8_t() const { return this->index_; } // NOLINT(google-explicit-constructor)

    [[nodiscard]] INLINE constexpr static uint8_t file(uint8_t index) { return index & 7; }
    [[nodiscard]] INLINE constexpr static uint8_t rank(uint8_t index) { return index >> 3; };

    [[nodiscard]] INLINE constexpr uint8_t file() const { return Square::file(this->index_); }
    [[nodiscard]] INLINE constexpr uint8_t rank() const { return Square::rank(this->index_); }

    [[nodiscard]] std::string uci() const;
    [[nodiscard]] std::string debugName() const;

private:
    uint8_t index_;
};

static_assert(sizeof(Square) == 1, "Square must be 1 byte");



// @formatter:off
namespace PieceTypeNamespace {
    enum PieceType {
        Pawn        = 0,
        Knight      = 1,
        Bishop      = 2,
        Rook        = 3,
        Queen       = 4,
        King        = 5,

        Empty       = 7
    };
}
using PieceType = PieceTypeNamespace::PieceType;

namespace PieceMaterial {
    constexpr int32_t Pawn          = 100;
    constexpr int32_t Knight        = 320;
    constexpr int32_t Bishop        = 330;
    constexpr int32_t Rook          = 500;
    constexpr int32_t Queen         = 900;
    constexpr int32_t King          = 1000000;
    constexpr std::array<int32_t, 6> Values = { Pawn, Knight, Bishop, Rook, Queen, King };

    constexpr int32_t BishopPair    = 50;

    constexpr int32_t RookOn7th     = 100;
}

namespace ColorNamespace {
    enum Color {
        White       = 0,
        Black       = 1
    };
}
using Color = ColorNamespace::Color;
// @formatter:on

INLINE constexpr Color operator~(Color color) {
    return static_cast<Color>(color ^ 1);
}

template<typename T>
class ColorMap {
public:
    ColorMap() = default;

    ColorMap(const ColorMap &other) = default;
    ColorMap &operator=(const ColorMap &other) = default;
    ColorMap(ColorMap &&other) noexcept = default;
    ColorMap &operator=(ColorMap &&other) noexcept = default;

    [[nodiscard]] INLINE auto &operator[](Color color) { return this->values_[color]; }
    [[nodiscard]] INLINE const auto &operator[](Color color) const { return this->values_[color]; }

    [[nodiscard]] INLINE auto &white() { return this->values_[0]; }
    [[nodiscard]] INLINE auto &black() { return this->values_[1]; }
    [[nodiscard]] INLINE const auto &white() const { return this->values_[0]; }
    [[nodiscard]] INLINE const auto &black() const { return this->values_[1]; }

private:
    std::array<T, 2> values_;
};



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

    [[nodiscard]] INLINE constexpr int32_t material() const { return PieceMaterial::Values[this->type()]; }

    [[nodiscard]] std::string debugName() const;

private:
    uint8_t bits_;
};

static_assert(sizeof(Piece) == 1, "Piece must be 1 byte");
