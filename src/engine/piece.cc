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



// @formatter:off
int32_t Piece::material() const {
    switch (this->type()) {
        case PieceType::Pawn: return PieceMaterial::Pawn;
        case PieceType::Knight: return PieceMaterial::Knight;
        case PieceType::Bishop: return PieceMaterial::Bishop;
        case PieceType::Rook: return PieceMaterial::Rook;
        case PieceType::Queen: return PieceMaterial::Queen;
        case PieceType::King: return PieceMaterial::King;
        default: return 0;
    }
}
// @formatter:on

std::string Piece::debugName() const {
    std::string name;
    if (this->color() == Color::White) {
        name += "White ";
    } else {
        name += "Black ";
    }

    switch (this->type()) {
        case PieceType::Pawn:
            name += "Pawn";
            break;
        case PieceType::Knight:
            name += "Knight";
            break;
        case PieceType::Bishop:
            name += "Bishop";
            break;
        case PieceType::Rook:
            name += "Rook";
            break;
        case PieceType::Queen:
            name += "Queen";
            break;
        case PieceType::King:
            name += "King";
            break;
        default:
            name = "Unknown";
            break;
    }

    return name;
}
