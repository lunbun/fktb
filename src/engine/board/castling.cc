#include "castling.h"

#include <vector>
#include <string>

Square CastlingRook::from(Color color, CastlingSide side) {
    // White king   -> H1 (7)
    // White queen  -> A1 (0)
    // Black king   -> H8 (63)
    // Black queen  -> A8 (56)

    // From this, we can derive
    return (color * 56) + ((2 - side) * 7);
}

Square CastlingRook::to(Color color, CastlingSide side) {
    // White king   -> F1 (5)
    // White queen  -> D1 (3)
    // Black king   -> F8 (61)
    // Black queen  -> D8 (59)

    // From this, we can derive
    return (color * 56) + ((2 - side) * 2) + 3;
}

CastlingRights CastlingRights::fromRookSquare(Square square) {
    // H1 (7    0b000111)   -> White king   (1  0b0001)
    // A1 (0    0b000000)   -> White queen  (2  0b0010)
    // H8 (63   0b111111)   -> Black king   (4  0b0100)
    // A8 (56   0b111000)   -> Black queen  (8  0b1000)

    // Because we can assume that the square is a rook square, the following properties are true:
    //      (square & 1) will tell us if it's a king or queen rook.             (0 = queen, 1 = king)
    //      ((square & 16) >> 4) will tell us if it's a white or black rook.    (0 = white, 1 = black)
    //
    // The rest is just using these values to format the bits correctly to conform to the CastlingRights enum.
    return (2 - (square & 1)) << (((square & 16) >> 4) * 2);
}

std::string join(const std::vector<std::string> &strings, const std::string &delimiter) {
    std::string result;

    for (size_t i = 0; i < strings.size(); i++) {
        result += strings[i];

        if (i != strings.size() - 1) {
            result += delimiter;
        }
    }

    return result;
}

// @formatter:off
std::string CastlingRights::debugName() const {
    std::vector<std::string> whiteRights, blackRights;
    if (this->has(CastlingRights::WhiteKingSide))   whiteRights.emplace_back("King");
    if (this->has(CastlingRights::WhiteQueenSide))  whiteRights.emplace_back("Queen");
    if (this->has(CastlingRights::BlackKingSide))   blackRights.emplace_back("King");
    if (this->has(CastlingRights::BlackQueenSide))  blackRights.emplace_back("Queen");

    return "White(" + join(whiteRights, ", ") + "), Black(" + join(blackRights, ", ") + ")";
}
// @formatter:on
