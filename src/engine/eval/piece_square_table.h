#pragma once

#include <cstdint>
#include <array>

#include "game_phase.h"
#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"

constexpr SquareMap<int16_t> flipVertical(const SquareMap<int16_t> &table) {
    SquareMap<int16_t> flippedTable = { 0 };
    for (uint8_t rank = 0; rank < 8; rank++) {
        for (uint8_t file = 0; file < 8; file++) {
            uint8_t index = rank * 8 + file;
            uint8_t flippedIndex = (7 - rank) * 8 + file;
            flippedTable[flippedIndex] = table[index];
        }
    }
    return flippedTable;
}

class PieceSquareTable {
public:
    constexpr PieceSquareTable(SquareMap<int16_t> middleGame, SquareMap<int16_t> endGame)
        : table_(ColorMap(flipVertical(middleGame), middleGame), ColorMap(flipVertical(endGame), endGame)) { }

    [[nodiscard]] INLINE constexpr const auto &operator[](GamePhase phase) const { return this->table_[phase]; }

    [[nodiscard]] INLINE constexpr int16_t interpolate(Color color, Square square, uint16_t phase) const {
        return static_cast<int16_t>(TaperedEval::interpolate(this->table_[GamePhase::Middle][color][square],
            this->table_[GamePhase::End][color][square], phase));
    }

private:
    GamePhaseMap<ColorMap<SquareMap<int16_t>>> table_;
};

namespace PieceSquareTables {
    extern const PieceSquareTable Pawn;
    extern const PieceSquareTable Knight;
    extern const PieceSquareTable Bishop;
    extern const PieceSquareTable Rook;
    extern const PieceSquareTable Queen;
    extern const PieceSquareTable King;
    extern const PieceTypeMap<const PieceSquareTable *> Tables;

    [[nodiscard]] INLINE int16_t evaluate(GamePhase phase, Piece piece, Square square) {
        return (*Tables[piece.type()])[phase][piece.color()][square];
    }
    [[nodiscard]] INLINE int16_t interpolate(Piece piece, Square square, uint16_t phase) {
        return (*Tables[piece.type()]).interpolate(piece.color(), square, phase);
    }
}
