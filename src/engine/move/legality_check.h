#pragma once

#include "move.h"
#include "engine/board/board.h"

namespace FKTB {

// Determines if a move of a given side is legal.
template<Color Side>
class LegalityChecker {
public:
    explicit LegalityChecker(Board &board);

    // Checks if a move is pseudo-legal.
    [[nodiscard]] bool isPseudoLegal(Move move) const;

    // Checks if a move is both pseudo-legal and legal.
    [[nodiscard]] bool isLegal(Move move) const;

private:
    Board &board_;

    Bitboard enemy_;
    Bitboard occupied_;
    Bitboard empty_;

    [[nodiscard]] bool isPseudoLegalQuiet(Move move, Piece piece) const;
    [[nodiscard]] bool isPseudoLegalDoublePawnPush(Move move, Piece piece) const;
    [[nodiscard]] bool isPseudoLegalKingCastle(Move move, Piece piece) const;
    [[nodiscard]] bool isPseudoLegalQueenCastle(Move move, Piece piece) const;
    [[nodiscard]] bool isPseudoLegalCapture(Move move, Piece piece, Piece captured) const;
    [[nodiscard]] bool isPseudoLegalEnPassant(Move move, Piece piece, Piece captured) const;
    template<bool IsCapture>
    [[nodiscard]] bool isPseudoLegalPromotion(Move move, Piece piece, Piece captured) const;

    [[nodiscard]] bool isLegalCastle(Move move) const;
};

namespace LegalityCheck {

// Checks if a move is both pseudo-legal and legal.
// Warning: This is slow because it constructs a LegalityChecker every call, and uses a branch to determine the side. In
// performance-critical code, use the LegalityChecker template directly.
[[nodiscard]] bool isLegal(Board &board, Move move);

} // namespace LegalityCheck

} // namespace FKTB
