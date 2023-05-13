#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

#include "piece.h"

class FenReader {
public:
    explicit FenReader(const std::string &fen);

    // Returns true if there are more pieces in the FEN string.
    [[nodiscard]] bool hasNext() const;
    // Moves to the next piece in the FEN string.
    Piece next();

    [[nodiscard]] PieceColor turn() const;

private:
    std::vector<std::string> fields_;

    std::string pieceField_;
    int pieceIndex_;
    int8_t file_, rank_;

    std::optional<Piece> nextPiece_;

    void readNextPiece();
};

class FenWriter {
public:
    FenWriter();

    // Adds a piece to the board.
    void piece(Piece piece);
    // Adds an empty square to the board.
    void empty();
    // Moves to the next rank.
    void nextRank();

    void setTurn(PieceColor color);

    // Returns the FEN string.
    [[nodiscard]] const std::string &fen() const { return this->fen_; }

private:
    int rank_, file_;

    int emptyFilesInARow_;

    std::string fen_;

    // Adds empty files to the FEN string if necessary.
    void maybeAddEmptyFilesToFen();
};
