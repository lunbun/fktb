#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <vector>

#include "piece.h"
#include "inline.h"

class Board;

// @formatter:off
namespace MoveFlagNamespace {
    enum MoveFlag : uint8_t {
        Quiet               = 0b0000,

        Capture             = 0b0100,

        PromotionMask       = 0b1000,
        KnightPromotion     = 0b1000,
        BishopPromotion     = 0b1001,
        RookPromotion       = 0b1010,
        QueenPromotion      = 0b1011,

        KnightPromoCapture  = KnightPromotion  | Capture,
        BishopPromoCapture  = BishopPromotion  | Capture,
        RookPromoCapture    = RookPromotion    | Capture,
        QueenPromoCapture   = QueenPromotion   | Capture,
    };
}
using MoveFlag = MoveFlagNamespace::MoveFlag;
// @formatter:on

class Move {
public:
    // Invalid moves have the same from and to squares.
    INLINE constexpr static Move invalid() { return { 0, 0, MoveFlag::Quiet }; }

    // Creates a move from a UCI string.
    static Move fromUci(const std::string &uci, const Board &board);

    INLINE constexpr Move(Square from, Square to, MoveFlag flags) : bits_((flags << 12) | (to << 6) | from) { }

    // Returns the square that the piece was moved from.
    [[nodiscard]] INLINE constexpr Square from() const { return this->bits_ & 0x3F; }
    // Returns the square that the piece was moved to.
    [[nodiscard]] INLINE constexpr Square to() const { return (this->bits_ >> 6) & 0x3F; }
    // Returns the flags for the move.
    [[nodiscard]] INLINE constexpr MoveFlag flags() const { return static_cast<MoveFlag>((this->bits_ >> 12) & 0x0F); }

    [[nodiscard]] INLINE constexpr bool isValid() const { return this->from() != this->to(); }
    [[nodiscard]] INLINE constexpr bool isCapture() const { return this->flags() & MoveFlag::Capture; }
    [[nodiscard]] INLINE constexpr bool isPromotion() const { return this->flags() & MoveFlag::PromotionMask; }

    // Returns the piece type that the pawn is promoted to.
    // Assumes that the move is a promotion, will return a garbage value otherwise.
    [[nodiscard]] INLINE constexpr PieceType promotion() const {
        return static_cast<PieceType>((this->flags() & 3) + PieceType::Knight);
    }

    [[nodiscard]] bool operator==(const Move &other) const;

    [[nodiscard]] std::string uci() const;
    [[nodiscard]] std::string debugName(const Board &board) const;

private:
    uint16_t bits_;
};

static_assert(sizeof(Move) == 2, "Move must be 2 bytes");
