#pragma once

#include <array>
#include <optional>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>
#include <cassert>

#include "color.h"
#include "square.h"
#include "piece.h"
#include "bitboard.h"
#include "engine/inline.h"
#include "engine/move/move.h"

struct MakeMoveInfo {
    uint64_t oldHash;
    CastlingRights oldCastlingRights;
    Square oldEnPassantSquare;
    Piece captured;

    MakeMoveInfo() = delete;
};

class Board {
public:
    constexpr static const char *StartingFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    // See https://www.chessprogramming.org/Perft_Results for some example FENs.
    constexpr static const char *KiwiPeteFen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ";
    constexpr static const char *EnPassantPinFen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ";
    constexpr static const char *ChecksAndPinsFen = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
    constexpr static const char *CapturedCastlingRookFen = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8";
    constexpr static const char *MirroredFen = "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10";
    constexpr static const char *PawnEndgameFen = "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1";

    Board(Color turn, CastlingRights castlingRights, Square enPassantSquare);
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
    template<Color Side>
    [[nodiscard]] INLINE int32_t pieceSquareEval() const { return this->pieceSquareEval_[Side]; }
    [[nodiscard]] INLINE Color turn() const { return this->turn_; }
    [[nodiscard]] INLINE uint64_t hash() const { return this->hash_; }
    [[nodiscard]] INLINE CastlingRights castlingRights() const { return this->castlingRights_; }
    [[nodiscard]] INLINE Square enPassantSquare() const { return this->enPassantSquare_; }

    template<Color Side>
    [[nodiscard]] INLINE Square king() const { return this->kings_[Side]; }
    [[nodiscard]] INLINE Piece pieceAt(Square square) const { return this->pieces_[square]; }

    // Returns the bitboard with all given pieces.
    [[nodiscard]] INLINE Bitboard &bitboard(PieceType pieceType, Color color) {
        assert(pieceType != PieceType::King);
        assert(pieceType != PieceType::Empty);
        return this->bitboards_[color][pieceType];
    }
    template<Color Side>
    [[nodiscard]] INLINE Bitboard bitboard(PieceType pieceType) const {
        assert(pieceType != PieceType::King);
        assert(pieceType != PieceType::Empty);
        return this->bitboards_[Side][pieceType];
    }

    // Returns the composite bitboard with all pieces of the given color.
    template<Color Side>
    [[nodiscard]] INLINE Bitboard composite() const {
        const auto &bitboards = this->bitboards_[Side];
        return bitboards[0] | bitboards[1] | bitboards[2] | bitboards[3] | bitboards[4] | (1ULL << this->king<Side>());
    }
    // Returns the bitboard of all occupied squares.
    [[nodiscard]] INLINE Bitboard occupied() const {
        return this->composite<Color::White>() | this->composite<Color::Black>();
    }
    // Returns the bitboard of all empty squares.
    [[nodiscard]] INLINE Bitboard empty() const { return ~this->occupied(); }

    // TODO: This function is expensive.
    template<Color Side>
    [[nodiscard]] bool isInCheck() const;

    template<bool UpdateHash>
    void addKing(Color color, Square square);
    template<bool UpdateHash>
    void addPiece(Piece piece, Square square);
    template<bool UpdateHash>
    void removePiece(Piece piece, Square square);

    // Makes/unmakes a move without updating the turn (updates the hash's turn, but not the turn_ field itself).
    template<bool UpdateTurn>
    MakeMoveInfo makeMove(Move move);
    template<bool UpdateTurn>
    void unmakeMove(Move move, MakeMoveInfo info);

private:
    ColorMap<int32_t> material_;
    ColorMap<int32_t> pieceSquareEval_;
    Color turn_;
    uint64_t hash_;

    ColorMap<Square> kings_;
    CastlingRights castlingRights_;
    Square enPassantSquare_;
    SquareMap<Piece> pieces_;
    ColorMap<std::array<Bitboard, 5>> bitboards_;

    // Updates the hash and castling rights.
    void castlingRights(CastlingRights newCastlingRights);

    // Removes the relevant castling rights if the given square is a rook square.
    void maybeRevokeCastlingRightsForRookSquare(Square square);

    // Updates the hash and en passant square.
    void enPassantSquare(Square newEnPassantSquare);

    // Moves a piece from one square to another.
    template<bool UpdateHash>
    void movePiece(Piece piece, Square from, Square to);

    // Moves/unmoves a piece from one square to another. Will not update hash if it is an unmake. Returns the moved
    // piece.
    template<bool IsMake>
    Piece moveOrUnmovePiece(Square from, Square to);

    // Makes/unmakes a castling move. Will not update hash or castling rights if it is an unmake.
    template<bool IsMake>
    void makeCastlingMove(Move move);

    // Makes/unmakes a promotion move. Will not update hash if it is an unmake.
    template<bool IsMake>
    void makePromotionMove(Move move);

    // Makes/unmakes a quiet move. Will not update hash or castling rights if it is an unmake.
    template<bool IsMake>
    void makeQuietMove(Move move);
};
