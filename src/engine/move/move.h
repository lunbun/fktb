#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <vector>

#include "engine/inline.h"
#include "engine/board/square.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/castling.h"

class Board;

// @formatter:off
namespace MoveFlagNamespace {
    enum MoveFlag : uint8_t {
        TacticalMask        = 0b1100,
        Quiet               = 0b0000,
        DoublePawnPush      = 0b0001,

        // IMPORTANT NOTE: Some promotion flags have the castle bits, so if you want to use CastleMask to check if a
        // move is a castle, you also have to make sure it's not a promotion.
        CastleMask          = 0b0010,
        KingCastle          = 0b0010,
        QueenCastle         = 0b0011,

        CaptureMask         = 0b0100,
        Capture             = 0b0100,
        EnPassant           = 0b0101,

        PromotionMask       = 0b1000,
        KnightPromotion     = 0b1000,
        BishopPromotion     = 0b1001,
        RookPromotion       = 0b1010,
        QueenPromotion      = 0b1011,

        KnightPromoCapture  = KnightPromotion  | CaptureMask,
        BishopPromoCapture  = BishopPromotion  | CaptureMask,
        RookPromoCapture    = RookPromotion    | CaptureMask,
        QueenPromoCapture   = QueenPromotion   | CaptureMask,
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
    [[nodiscard]] INLINE constexpr bool isQuiet() const { return !this->isTactical(); }
    [[nodiscard]] INLINE constexpr bool isTactical() const { return this->flags() & MoveFlag::TacticalMask; }
    [[nodiscard]] INLINE constexpr bool isDoublePawnPush() const { return this->flags() == MoveFlag::DoublePawnPush; }
    [[nodiscard]] INLINE constexpr bool isCastle() const {
        // Some promotion flags also have the castle bits, so we have to explicitly check that it's not a promotion.
        return this->flags() & MoveFlag::CastleMask && !this->isPromotion();
    }
    [[nodiscard]] INLINE constexpr bool isCapture() const { return this->flags() & MoveFlag::CaptureMask; }
    [[nodiscard]] INLINE constexpr bool isEnPassant() const { return this->flags() == MoveFlag::EnPassant; }
    [[nodiscard]] INLINE constexpr bool isPromotion() const { return this->flags() & MoveFlag::PromotionMask; }

    // Returns the square of the captured piece.
    // For normal capture moves, this is just the square that the piece was moved to. For en passant moves, this is the square of
    // the captured pawn.
    [[nodiscard]] INLINE constexpr Square capturedSquare() const;

    // Returns the square of the captured pawn in an en passant move.
    // Assumes that the move is an en passant, will return a garbage value otherwise.
    [[nodiscard]] INLINE constexpr Square enPassantCapturedSquare() const;

    // Returns the side that the castle is on.
    // Assumes that the move is a castle, will return a garbage value otherwise.
    [[nodiscard]] INLINE constexpr CastlingSide castlingSide() const;

    // Returns the piece type that the pawn is promoted to.
    // Assumes that the move is a promotion, will return a garbage value otherwise.
    [[nodiscard]] INLINE constexpr PieceType promotion() const;

    [[nodiscard]] INLINE constexpr bool operator==(Move other) const { return this->bits_ == other.bits_; }

    [[nodiscard]] std::string uci() const;
    [[nodiscard]] std::string debugName(const Board &board) const;

private:
    uint16_t bits_;
};

INLINE constexpr Square Move::capturedSquare() const {
    // TODO: Is branching here bad?
    return this->isEnPassant() ? this->enPassantCapturedSquare() : this->to();
}

INLINE constexpr Square Move::enPassantCapturedSquare() const {
    return { this->to().file(), this->from().rank() };
}

INLINE constexpr CastlingSide Move::castlingSide() const {
    return static_cast<CastlingSide>((this->flags() & 1) + CastlingSide::King);
}

INLINE constexpr PieceType Move::promotion() const {
    return static_cast<PieceType>((this->flags() & 3) + PieceType::Knight);
}

static_assert(sizeof(Move) == 2, "Move must be 2 bytes");
