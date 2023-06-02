#include "evaluation.h"

#include <cstdint>
#include <array>

#include "engine/inline.h"
#include "engine/board/bitboard.h"

constexpr PieceSquareTable flipVertical(const PieceSquareTable &table) {
    std::array<int32_t, 64> flippedTable = { 0 };
    for (uint8_t rank = 0; rank < 8; rank++) {
        for (uint8_t file = 0; file < 8; file++) {
            uint8_t index = rank * 8 + file;
            uint8_t flippedIndex = (7 - rank) * 8 + file;
            flippedTable[flippedIndex] = table[index];
        }
    }
    return flippedTable;
}

constexpr ColorMap<PieceSquareTable> createColoredPieceSquareTable(const PieceSquareTable &table) {
    // The table is from the perspective of the black side, so we need to flip it vertically to get the white side.
    return { flipVertical(table), table };
}

// Tables taken from https://www.chessprogramming.org/Simplified_Evaluation_Function
//
// Important Note: The tables are actually flipped vertically from what is visually shown here since index 0 (rank 1)
// is at the top, and index 63 (rank 8) is at the bottom. As a result, these tables are actually from the perspective
// of the black side.

// @formatter:off
constexpr ColorMap<PieceSquareTable> Evaluation::pawnTable = createColoredPieceSquareTable({
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0
});
constexpr ColorMap<PieceSquareTable> Evaluation::knightTable = createColoredPieceSquareTable({
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
});
constexpr ColorMap<PieceSquareTable> Evaluation::bishopTable = createColoredPieceSquareTable({
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
});
constexpr ColorMap<PieceSquareTable> Evaluation::rookTable = createColoredPieceSquareTable({
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0
});
constexpr ColorMap<PieceSquareTable> Evaluation::queenTable = createColoredPieceSquareTable({
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
    0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
});
constexpr ColorMap<PieceSquareTable> Evaluation::kingTable = createColoredPieceSquareTable({
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
});
// @formatter:on



template<Color Side, PieceType Type, const ColorMap<PieceSquareTable> &TableColorMap>
INLINE int32_t evaluatePieceTable(const Board &board) {
    constexpr const PieceSquareTable &Table = TableColorMap[Side];

    int32_t score = 0;

    Bitboard pieces = board.bitboard<Side>(Type);
    while (pieces) {
        Square square = pieces.bsfReset();
        score += Table[square];
    }

    return score;
}

template<Color Side>
INLINE int32_t evaluateKingTable(const Board &board) {
    constexpr const PieceSquareTable &Table = Evaluation::kingTable[Side];
    return Table[board.king<Side>()];
}

// Evaluates the board for the given side.
template<Color Side>
INLINE int32_t evaluateForSide(const Board &board) {
    int32_t score = 0;

    // Material
    score += board.material<Side>();

    // Bonus for having a bishop pair
    score += PieceMaterial::BishopPair * (board.bitboard<Side>(PieceType::Bishop).count() >= 2);

    score += evaluatePieceTable<Side, PieceType::Pawn, Evaluation::pawnTable>(board);
    score += evaluatePieceTable<Side, PieceType::Knight, Evaluation::knightTable>(board);
    score += evaluatePieceTable<Side, PieceType::Bishop, Evaluation::bishopTable>(board);
    score += evaluatePieceTable<Side, PieceType::Rook, Evaluation::rookTable>(board);
    score += evaluatePieceTable<Side, PieceType::Queen, Evaluation::queenTable>(board);
    score += evaluateKingTable<Side>(board);

    return score;
}

// Evaluates the board for the given side, subtracting the evaluation for the other side.
template<Color Turn>
int32_t Evaluation::evaluate(const Board &board) {
    return evaluateForSide<Turn>(board) - evaluateForSide<~Turn>(board);
}



template int32_t Evaluation::evaluate<Color::White>(const Board &board);
template int32_t Evaluation::evaluate<Color::Black>(const Board &board);
