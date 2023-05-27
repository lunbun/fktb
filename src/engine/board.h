#pragma once

#include <array>
#include <optional>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>
#include <cassert>

#include "piece.h"
#include "move.h"
#include "bitboard.h"
#include "inline.h"

struct MakeMoveInfo {
    Piece captured;
};

class Board {
public:
    constexpr static const char *StartingFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    explicit Board(Color turn);
    ~Board();

    Board(const Board &other) = delete;
    Board &operator=(const Board &other) = delete;
    Board(Board &&other) = default;
    Board &operator=(Board &&other) = default;

    static Board startingPosition();
    static Board fromFen(const std::string &fen);

    [[nodiscard]] std::string toFen() const;

    [[nodiscard]] Board copy() const;

    template<Color Side>
    [[nodiscard]] INLINE int32_t material() const { return this->material_[Side]; }
    [[nodiscard]] INLINE Color turn() const { return this->turn_; }
    [[nodiscard]] INLINE uint64_t &hash() { return this->hash_; }

    [[nodiscard]] INLINE Piece pieceAt(Square square) const { return this->pieces_[square]; }

    // Returns the bitboard with all given pieces.
    [[nodiscard]] INLINE Bitboard &bitboard(PieceType pieceType, Color color) {
        return this->bitboards_[color][pieceType];
    }
    template<Color Side>
    [[nodiscard]] Bitboard bitboard(PieceType pieceType) const { return this->bitboards_[Side][pieceType]; }

    // Returns the composite bitboard with all pieces of the given color.
    template<Color Side>
    [[nodiscard]] Bitboard composite() const;
    // Returns the bitboard of all occupied squares.
    [[nodiscard]] Bitboard occupied() const;
    // Returns the bitboard of all empty squares.
    [[nodiscard]] Bitboard empty() const;

    void addPiece(Piece piece, Square square);
    void removePiece(Piece piece, Square square);

    MakeMoveInfo makeMove(Move move);
    void unmakeMove(Move move, MakeMoveInfo info);

private:
    ColorMap<int32_t> material_;
    Color turn_;
    uint64_t hash_;

    std::array<Piece, 64> pieces_;
    ColorMap<std::array<Bitboard, 6>> bitboards_;
};
