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
