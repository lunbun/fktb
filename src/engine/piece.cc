#include "piece.h"

#include <string>

std::string Square::uci() const {
    std::string name;

    name += static_cast<char>(this->file + 'a');
    name += static_cast<char>(this->rank + '1');

    return name;
}

std::string Square::debugName() const {
    return this->uci();
}



Piece::Piece(PieceType type, PieceColor color, Square square) {
    this->type_ = static_cast<uint8_t>(type);
    this->color_ = static_cast<uint8_t>(color);
    this->file_ = static_cast<uint8_t>(square.file);
    this->rank_ = static_cast<uint8_t>(square.rank);
}

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

std::string Piece::debugName() const {
    std::string name;
    if (this->color() == PieceColor::White) {
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
