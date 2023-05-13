#pragma once

#include <cstdint>
#include <string>
#include <array>

struct Square {
    int8_t file;
    int8_t rank;

    [[nodiscard]] inline uint8_t index() const { return this->file + this->rank * 8; }

    [[nodiscard]] inline bool isInBounds() const {
        return this->file >= 0 && this->file < 8 && this->rank >= 0 && this->rank < 8;
    }

    [[nodiscard]] inline Square offset(int8_t offsetFile, int8_t offsetRank) const {
        return { static_cast<int8_t>(this->file + offsetFile), static_cast<int8_t>(this->rank + offsetRank) };
    }

    [[nodiscard]] std::string debugName() const;
};

static_assert(sizeof(Square) == 2, "Square must be 2 bytes");



// @formatter:off
enum class PieceType {
    Pawn   = 0,
    Knight = 1,
    Bishop = 2,
    Rook   = 3,
    Queen  = 4,
    King   = 5
};

enum class PieceColor {
    White  = 0,
    Black  = 1,
};
// @formatter:on

inline PieceColor operator~(PieceColor color) {
    return static_cast<PieceColor>(static_cast<uint8_t>(color) ^ 1);
}

// Direction the pawn moves in (i.e. how many ranks the pawn moves).
inline int8_t getPawnDirection(PieceColor color) {
    return static_cast<int8_t>(color == PieceColor::White ? 1 : -1);
}

template<typename T>
class ColorMap {
public:
    ColorMap() = default;

    ColorMap(const ColorMap &other) = default;
    ColorMap &operator=(const ColorMap &other) = default;
    ColorMap(ColorMap &&other)  noexcept = default;
    ColorMap &operator=(ColorMap &&other)  noexcept = default;

    [[nodiscard]] inline auto &operator[](PieceColor color) { return this->values_[static_cast<uint8_t>(color)]; }
    [[nodiscard]] inline const auto &operator[](PieceColor color) const {
        return this->values_[static_cast<uint8_t>(color)];
    }

    [[nodiscard]] inline auto &white() { return this->values_[0]; }
    [[nodiscard]] inline auto &black() { return this->values_[1]; }
    [[nodiscard]] inline const auto &white() const { return this->values_[0]; }
    [[nodiscard]] inline const auto &black() const { return this->values_[1]; }

private:
    std::array<T, 2> values_;
};



struct Piece {
    Piece(PieceType type, PieceColor color, Square square);

    [[nodiscard]] inline PieceColor color() const { return static_cast<PieceColor>(this->color_); }
    [[nodiscard]] inline PieceType type() const { return static_cast<PieceType>(this->type_); }
    [[nodiscard]] inline int8_t file() const { return static_cast<int8_t>(this->file_); }
    [[nodiscard]] inline int8_t rank() const { return static_cast<int8_t>(this->rank_); }
    [[nodiscard]] inline Square square() const {
        return { static_cast<int8_t>(this->file_), static_cast<int8_t>(this->rank_) };
    }

    inline void setSquare(Square square) {
        this->file_ = static_cast<uint8_t>(square.file);
        this->rank_ = static_cast<uint8_t>(square.rank);
    }

    [[nodiscard]] int32_t material() const;

    [[nodiscard]] std::string debugName() const;

private:
    uint8_t color_ : 1;
    uint8_t type_ : 3;
    uint8_t file_ : 3;
    uint8_t rank_ : 3;
};

static_assert(sizeof(Piece) == 2, "Piece must be 2 bytes");
