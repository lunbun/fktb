#include "piece.h"

#include <string>
#include <vector>

#include "color.h"

std::string PieceTypeNamespace::debugName(PieceType pieceType) {
    // @formatter:off
    switch (pieceType) {
        case PieceType::Pawn:   return "Pawn";
        case PieceType::Knight: return "Knight";
        case PieceType::Bishop: return "Bishop";
        case PieceType::Rook:   return "Rook";
        case PieceType::Queen:  return "Queen";
        case PieceType::King:   return "King";
        case PieceType::Empty:  return "Empty";
        default:                return "Unknown";
    }
    // @formatter:on
}

std::string Piece::debugName() const {
    return ColorNamespace::debugName(this->color()) + " " + PieceTypeNamespace::debugName(this->type());
}
