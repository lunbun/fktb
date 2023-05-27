#include "move.h"

#include <stdexcept>
#include <cstring>

#include "board.h"

MoveFlag promotionFlagFromUciChar(char c) {
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

char pieceTypeToUciChar(PieceType type) {
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

    uint8_t flags = MoveFlag::Quiet;

    Piece captured = board.pieceAt(to);
    if (!captured.isEmpty()) {
        flags |= MoveFlag::Capture;
    }

    if (uci.length() == 5) {
        flags |= promotionFlagFromUciChar(uci[4]);
    }

    return { from, to, static_cast<MoveFlag>(flags) };
}



bool Move::operator==(const Move &other) const {
    return !std::memcmp(this, &other, sizeof(Move));
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

    if (this->isCapture()) {
        Piece captured = board.pieceAt(this->to());

        name += " capturing ";
        name += captured.debugName();
    }

    if (this->isPromotion()) {
        name += " promoting to ";
        name += PieceTypeNamespace::debugName(this->promotion());
    }

    return name;
}
