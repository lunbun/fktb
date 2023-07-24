#include "move.h"

#include <stdexcept>
#include <cstring>

#include "engine/inline.h"
#include "engine/board/board.h"

namespace FKTB {

namespace {

INLINE MoveFlag promotionFlagFromUciChar(char c) {
    // @formatter:off
    switch (c) {
        case 'n': return MoveFlag::KnightPromotion;
        case 'b': return MoveFlag::BishopPromotion;
        case 'r': return MoveFlag::RookPromotion;
        case 'q': return MoveFlag::QueenPromotion;
        default: throw std::invalid_argument("Invalid piece type character");
    }
    // @formatter:on
}

INLINE char pieceTypeToUciChar(PieceType type) {
    // @formatter:off
    switch (type) {
        case PieceType::Knight: return 'n';
        case PieceType::Bishop: return 'b';
        case PieceType::Rook:   return 'r';
        case PieceType::Queen:  return 'q';
        default: throw std::invalid_argument("Invalid piece type");
    }
    // @formatter:on
}

} // namespace

Move Move::fromUci(const std::string &uci, const Board &board) {
    if (uci.length() != 4 && uci.length() != 5) {
        throw std::invalid_argument("Move string must be 4 or 5 characters long");
    }

    auto fromFile = static_cast<int8_t>(uci[0] - 'a');
    auto fromRank = static_cast<int8_t>(uci[1] - '1');
    auto toFile = static_cast<int8_t>(uci[2] - 'a');
    auto toRank = static_cast<int8_t>(uci[3] - '1');

    Square from(fromFile, fromRank);
    Square to(toFile, toRank);

    Piece piece = board.pieceAt(from);

    uint8_t flags = MoveFlag::Quiet;

    // Check for double pawn push
    if (piece.type() == PieceType::Pawn) {
        int32_t rankDiff = to.rank() - from.rank();
        if (std::abs(rankDiff) == 2) {
            flags |= MoveFlag::DoublePawnPush;
        }
    }

    // Check for castling
    if (piece.type() == PieceType::King) {
        int32_t fileDiff = to.file() - from.file();
        if (fileDiff == 2) {
            flags |= MoveFlag::KingCastle;
        } else if (fileDiff == -2) {
            flags |= MoveFlag::QueenCastle;
        }
    }

    // Check for captures
    Piece captured = board.pieceAt(to);
    if (!captured.isEmpty()) {
        flags |= MoveFlag::Capture;
    }

    // Check for en passant
    if (piece.type() == PieceType::Pawn && to == board.enPassantSquare()) {
        flags |= MoveFlag::EnPassant;
    }

    // Check for promotions
    if (uci.length() == 5) {
        flags |= promotionFlagFromUciChar(uci[4]);
    }

    return { from, to, static_cast<MoveFlag>(flags) };
}



std::string Move::uci() const {
    std::string uci;

    uci += this->from().uci();
    uci += this->to().uci();

    if (this->isPromotion()) {
        uci += pieceTypeToUciChar(this->promotion());
    }

    return uci;
}

std::string Move::debugName(const Board &board) const {
    std::string name;

    Piece piece = board.pieceAt(this->from());
    name += piece.debugName();
    name += " from ";
    name += this->from().debugName();
    name += " to ";
    name += this->to().debugName();

    if (this->isCastle()) {
        name += " castling";
    }

    if (this->isCapture()) {
        Piece captured = board.pieceAt(this->capturedSquare());

        name += " capturing ";
        name += captured.debugName();

        if (this->isEnPassant()) {
            name += " en passant";
        }
    }

    if (this->isPromotion()) {
        name += " promoting to ";
        name += PieceTypeNamespace::debugName(this->promotion());
    }

    return name;
}

} // namespace FKTB
