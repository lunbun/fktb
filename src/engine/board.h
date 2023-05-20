#pragma once

#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

#include "piece.h"
#include "move.h"
#include "transposition.h"

class Board {
public:
    static constexpr const char *STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    explicit Board(PieceColor turn);
    ~Board();

    Board(const Board &other) = delete;
    Board &operator=(const Board &other) = delete;
    Board(Board &&other) = default;
    Board &operator=(Board &&other) = default;

    static Board startingPosition();
    static Board fromFen(const std::string &fen);

    [[nodiscard]] std::string toFen() const;

    [[nodiscard]] Board copy() const;

    [[nodiscard]] inline int32_t material(PieceColor color) const { return this->material_[color]; }
    [[nodiscard]] inline PieceColor turn() const { return this->turn_; }
    [[nodiscard]] inline ZobristHash &hash() { return this->hash_; }

    [[nodiscard]] inline const ColorMap<std::vector<std::unique_ptr<Piece>>> &pawns() const { return this->pawns_; }
    [[nodiscard]] inline const ColorMap<std::vector<std::unique_ptr<Piece>>> &knights() const { return this->knights_; }
    [[nodiscard]] inline const ColorMap<std::vector<std::unique_ptr<Piece>>> &bishops() const { return this->bishops_; }
    [[nodiscard]] inline const ColorMap<std::vector<std::unique_ptr<Piece>>> &rooks() const { return this->rooks_; }
    [[nodiscard]] inline const ColorMap<std::vector<std::unique_ptr<Piece>>> &queens() const { return this->queens_; }
    [[nodiscard]] inline const ColorMap<std::vector<std::unique_ptr<Piece>>> &kings() const { return this->kings_; }

    void addPiece(Piece piece);
    void removePiece(Piece *piece);

    [[nodiscard]] Piece *getPieceAt(Square square) const;

    void makeMove(Move move);
    void unmakeMove(Move move);

private:
    ColorMap<int32_t> material_;
    PieceColor turn_;
    ZobristHash hash_;

    ColorMap<std::vector<std::unique_ptr<Piece>>> pawns_;
    ColorMap<std::vector<std::unique_ptr<Piece>>> knights_;
    ColorMap<std::vector<std::unique_ptr<Piece>>> bishops_;
    ColorMap<std::vector<std::unique_ptr<Piece>>> rooks_;
    ColorMap<std::vector<std::unique_ptr<Piece>>> queens_;
    ColorMap<std::vector<std::unique_ptr<Piece>>> kings_;

    // For quick lookup to pieces by square. This does not own the pointers.
    std::array<Piece *, 64> squares_;

    std::vector<std::unique_ptr<Piece>> &getPieceList(Piece piece);

    void addPieceNoHashUpdate(Piece piece);
    void removePieceNoHashUpdate(Piece *piece);
};
