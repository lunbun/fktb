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
#include "engine/eval/game_phase.h"

namespace FKTB {

// @formatter:off

// Gives special control over what to update when making a move. Some calls to makeMove may not need all information to be
// updated, so this can be used to avoid unnecessary work.
//
// Note: The piece array (Board::pieces_) is always updated regardless of the flags, since if it is not, the internal board state
// can become corrupted.
namespace MakeMoveFlags {
    constexpr uint32_t Turn         = 0x01;             // Update the turn (will still update the hash's turn even if this is not
                                                        // passed)?
    constexpr uint32_t Gameplay     = 0x02;             // Update gameplay information (castling rights, en passant square, etc.)?
    constexpr uint32_t Hash         = 0x04 | Gameplay;  // Update the Zobrist hash?
                                                        // Note: Hash requires the Gameplay flag, because otherwise, the hash will
                                                        // not synchronize with changes in gameplay information.
    constexpr uint32_t Evaluation   = 0x08;             // Update the evaluation (material and piece-square tables)?
    constexpr uint32_t Bitboards    = 0x10;             // Update the bitboards?
    constexpr uint32_t Repetition   = 0x20;             // Update the three-fold repetition hash list?

    constexpr uint32_t Unmake       = 0x40;             // Flag used internally to indicate a move is being unmade. Do not pass this flag
                                                        // to makeMove/unmakeMove.
} // namespace MakeMoveFlags

// Only these sets of flags will link properly with the makeMove/unmakeMove methods.
namespace MakeMoveType {
    // Makes a move and updates all information.
    constexpr uint32_t All                  = MakeMoveFlags::Turn | MakeMoveFlags::Gameplay | MakeMoveFlags::Hash |
                                              MakeMoveFlags::Evaluation | MakeMoveFlags::Bitboards | MakeMoveFlags::Repetition;

    // Makes a move and updates all information except the turn.
    constexpr uint32_t AllNoTurn            = MakeMoveFlags::Gameplay | MakeMoveFlags::Hash | MakeMoveFlags::Evaluation |
                                              MakeMoveFlags::Bitboards | MakeMoveFlags::Repetition;

    // Makes a move and updates only the bitboards.
    constexpr uint32_t BitboardsOnly        = MakeMoveFlags::Bitboards;
} // namespace MakeMoveType

// @formatter:on

struct MakeMoveInfo {
    uint64_t oldHash;
    uint32_t oldPliesSinceIrreversible;
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

    [[nodiscard]] INLINE int32_t material(Color color) const { return this->material_[color]; }
    [[nodiscard]] INLINE int32_t pieceSquareEval(GamePhase phase, Color color) const;
    [[nodiscard]] INLINE Color turn() const { return this->turn_; }
    [[nodiscard]] INLINE uint64_t hash() const { return this->hash_; }
    [[nodiscard]] INLINE CastlingRights castlingRights() const { return this->castlingRights_; }
    [[nodiscard]] INLINE Square enPassantSquare() const { return this->enPassantSquare_; }

    [[nodiscard]] INLINE Square king(Color color) const { return this->kings_[color]; }
    [[nodiscard]] INLINE Piece pieceAt(Square square) const { return this->pieces_[square]; }

    // Returns the bitboard with all given pieces.
    //
    // No need to worry about performance losses with constructing lots of Piece objects to pass to this function; GCC with -O3
    // can optimize away the construction if the piece is constant (tested on godbolt).
    [[nodiscard]] INLINE Bitboard &bitboard(Piece piece);
    [[nodiscard]] INLINE Bitboard bitboard(Piece piece) const;

    // Returns the composite bitboard with all pieces of the given color.
    [[nodiscard]] INLINE Bitboard composite(Color color) const;
    // Returns the composite bitboard with all pieces of the given type.
    [[nodiscard]] INLINE Bitboard composite(PieceType type) const;
    // Returns the bitboard of all occupied squares.
    [[nodiscard]] INLINE Bitboard occupied() const { return this->composite(Color::White) | this->composite(Color::Black); }
    // Returns the bitboard of all empty squares.
    [[nodiscard]] INLINE Bitboard empty() const { return ~this->occupied(); }

    // TODO: This function is expensive.
    template<Color Side>
    [[nodiscard]] bool isInCheck() const;

    // Returns true if this position has existed before in the game.
    [[nodiscard]] bool isTwofoldRepetition() const;

    template<uint32_t Flags>
    void addKing(Color color, Square square);
    template<uint32_t Flags>
    void addPiece(Piece piece, Square square);
    template<uint32_t Flags>
    void removePiece(Piece piece, Square square);

    // Makes/unmakes a move.
    template<uint32_t Flags>
    MakeMoveInfo makeMove(Move move);
    template<uint32_t Flags>
    void unmakeMove(Move move, MakeMoveInfo info);

    // Makes/unmakes a null move without updating the turn.
    MakeMoveInfo makeNullMove();
    void unmakeNullMove(MakeMoveInfo info);

private:
    ColorMap<int32_t> material_;
    GamePhaseMap<ColorMap<int32_t>> pieceSquareEval_;
    Color turn_;
    uint64_t hash_;

    std::vector<uint64_t> repetitionHashes_;
    uint32_t pliesSinceIrreversible_;

    CastlingRights castlingRights_;
    Square enPassantSquare_;

    ColorMap<Square> kings_;
    SquareMap<Piece> pieces_;
    ColorMap<std::array<Bitboard, 5>> bitboards_;

    // Updates the hash and castling rights.
    template<uint32_t Flags>
    void castlingRights(CastlingRights newCastlingRights);

    // Removes the relevant castling rights if the given square is a rook square.
    template<uint32_t Flags>
    void maybeRevokeCastlingRightsForRookSquare(Square square);

    // Updates the hash and en passant square.
    template<uint32_t Flags>
    void enPassantSquare(Square newEnPassantSquare);

    // Updates the repetition hashes for making a move.
    template<uint32_t Flags>
    void updateRepetitionHashes(Move move);

    // Moves/unmoves a piece from one square to another. Returns the piece that was moved.
    template<uint32_t Flags>
    Piece movePiece(Square from, Square to);

    // Makes/unmakes a castling move.
    template<uint32_t Flags>
    void makeCastlingMove(Move move);

    // Makes/unmakes a promotion move.
    template<uint32_t Flags>
    void makePromotionMove(Move move);

    // Makes/unmakes a quiet move.
    template<uint32_t Flags>
    void makeQuietMove(Move move);
};



INLINE int32_t Board::pieceSquareEval(GamePhase phase, Color color) const {
    return this->pieceSquareEval_[phase][color];
}

INLINE Bitboard &Board::bitboard(Piece piece) {
    assert(piece.type() != PieceType::King);
    assert(piece.type() != PieceType::Empty);
    return this->bitboards_[piece.color()][piece.type()];
}

INLINE Bitboard Board::bitboard(Piece piece) const {
    assert(piece.type() != PieceType::King);
    assert(piece.type() != PieceType::Empty);
    return this->bitboards_[piece.color()][piece.type()];
}

Bitboard Board::composite(Color color) const {
    const auto &bitboards = this->bitboards_[color];
    return bitboards[0] | bitboards[1] | bitboards[2] | bitboards[3] | bitboards[4] | (1ULL << this->king(color));
}

Bitboard Board::composite(PieceType type) const {
    return this->bitboards_[Color::White][type] | this->bitboards_[Color::Black][type];
}

} // namespace FKTB
