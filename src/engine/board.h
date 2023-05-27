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
    [[nodiscard]] INLINE Bitboard bitboard(PieceType pieceType) const { return this->bitboards_[Side][pieceType]; }

    // Returns the composite bitboard with all pieces of the given color.
    template<Color Side>
    [[nodiscard]] INLINE Bitboard composite() const {
        const auto &bitboards = this->bitboards_[Side];
        return bitboards[0] | bitboards[1] | bitboards[2] | bitboards[3] | bitboards[4] | bitboards[5];
    }
    // Returns the bitboard of all occupied squares.
    [[nodiscard]] INLINE Bitboard occupied() const {
        return this->composite<Color::White>() | this->composite<Color::Black>();
    }
    // Returns the bitboard of all empty squares.
    [[nodiscard]] INLINE Bitboard empty() const { return ~this->occupied(); }

    void addPiece(Piece piece, Square square);
    void removePiece(Piece piece, Square square);

    // Makes/unmakes a move without updating the turn (updates the hash's turn, but not the turn_ field itself).
    MakeMoveInfo makeMoveNoTurnUpdate(Move move);
    void unmakeMoveNoTurnUpdate(Move move, MakeMoveInfo info);

    MakeMoveInfo makeMove(Move move);
    void unmakeMove(Move move, MakeMoveInfo info);

private:
    ColorMap<int32_t> material_;
    Color turn_;
    uint64_t hash_;

    std::array<Piece, 64> pieces_;
    ColorMap<std::array<Bitboard, 6>> bitboards_;
};
