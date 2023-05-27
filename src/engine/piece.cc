#include "piece.h"

#include <string>

std::string Square::uci() const {
    std::string name;

    name += static_cast<char>(this->file() + 'a');
    name += static_cast<char>(this->rank() + '1');

    return name;
}

std::string Square::debugName() const {
    return this->uci();
}

std::string PieceTypeNamespace::debugName(PieceType pieceType) {
    // @formatter:off
    switch (pieceType) {
        case PieceType::Pawn: return "Pawn";
        case PieceType::Knight: return "Knight";
        case PieceType::Bishop: return "Bishop";
        case PieceType::Rook: return "Rook";
        case PieceType::Queen: return "Queen";
        case PieceType::King: return "King";
        default: return "Unknown";
    }
    // @formatter:on
}

std::string ColorNamespace::debugName(Color color) {
    // @formatter:off
    switch (color) {
        case Color::White: return "White";
        case Color::Black: return "Black";
        default: return "Unknown";
    }
    // @formatter:on
}

std::string Piece::debugName() const {
    return ColorNamespace::debugName(this->color()) + " " + PieceTypeNamespace::debugName(this->type());
}
