#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

#include "square.h"
#include "piece.h"
#include "castling.h"

class FenReader {
public:
    struct Entry {
        Piece piece;
        Square square;

        Entry() = delete;
    };

    explicit FenReader(const std::string &fen);

    // Returns true if there are more pieces in the FEN string.
    [[nodiscard]] bool hasNext() const;
    // Moves to the next piece in the FEN string.
    Entry next();

    [[nodiscard]] Color turn() const;
    [[nodiscard]] CastlingRights castlingRights() const;

private:
    std::vector<std::string> fields_;

    std::string pieceField_;
    int pieceIndex_;
    int8_t file_, rank_;

    std::optional<Entry> nextPiece_;

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

    void turn(Color color);
    void castlingRights(CastlingRights castlingRights);

    // Returns the FEN string.
    [[nodiscard]] const std::string &fen() const { return this->fen_; }

private:
    int rank_, file_;

    int emptyFilesInARow_;

    std::string fen_;

    // Adds empty files to the FEN string if necessary.
    void maybeAddEmptyFilesToFen();
};
