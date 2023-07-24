#include "bitboard.h"

#include <string>
#include <array>

#include "color.h"
#include "square.h"
#include "piece.h"
#include "board.h"
#include "engine/inline.h"
#include "engine/intrinsics.h"

namespace FKTB {

std::string Bitboard::debug() const {
    std::string result;
    for (int8_t rank = 7; rank >= 0; --rank) {
        for (int8_t file = 0; file < 8; ++file) {
            result += this->get(Square(file, rank)) ? '1' : '.';

            if (file != 7) {
                result += "  ";
            }
        }

        if (rank != 0) {
            result += '\n';
        }
    }
    return result;
}



namespace {

// Table of bitboards with all the squares between two squares.
// See https://www.chessprogramming.org/Square_Attacked_By#0x88_Difference
std::array<Bitboard, 240> between0x88Table;

// Using PEXT bitboards for generating sliding piece attacks: https://www.chessprogramming.org/BMI2#PEXTBitboards
// (similar to magic bitboards, but without the magic :) )
struct PextTableEntry {
    Bitboard occupiedMask;  // The mask of occupied squares.
    Bitboard attackMask;    // Attacks on an empty board.
    uint32_t offset;        // The offset into the attack table.

    PextTableEntry() : occupiedMask(0), attackMask(0), offset(0) { }
    PextTableEntry(Bitboard occupiedMask, Bitboard attackMask, uint32_t offset) : occupiedMask(occupiedMask),
                                                                                  attackMask(attackMask),
                                                                                  offset(offset) { }
};

// Total 213.25 KiB
SquareMap<PextTableEntry> diagonalPextTable;        // 1.5 KiB
SquareMap<PextTableEntry> orthogonalPextTable;      // 1.5 KiB
std::array<uint16_t, 107648> slidingAttackTable;    // 210.25 KiB

ColorMap<SquareMap<Bitboard>> pawnAttackTable;
SquareMap<Bitboard> knightAttackTable;
SquareMap<Bitboard> kingAttackTable;



// No clue what this does but see https://www.chessprogramming.org/Square_Attacked_By#0x88_Difference
INLINE uint8_t x88Diff(Square from, Square to) {
    return to - from + (to | 7) - (from | 7) + 120;
}

// This is an expensive method, only call during initialization.
Bitboard generateBetween(Square a, Square b) {
    // Copied from https://www.chessprogramming.org/Square_Attacked_By#Pure_Calculation
    // @formatter:off
    constexpr uint64_t m1   = -1ULL;
    constexpr uint64_t a2a7 = 0x0001010101010100ULL;
    constexpr uint64_t b2g7 = 0x0040201008040200ULL;
    constexpr uint64_t h1b7 = 0x0002040810204080ULL; /* Thanks Dustin, g2b7 did not work for c1-a3 */
    uint64_t btwn, line, rank, file;

    btwn  = (m1 << a) ^ (m1 << b);
    file  =   (b & 7) - (a   & 7);
    rank  =  ((b | 7) -  a) >> 3 ;
    line  =      (   (file  &  7) - 1) & a2a7; /* a2a7 if same file */
    line += 2 * ((   (rank  &  7) - 1) >> 58); /* b1g1 if same rank */
    line += (((rank - file) & 15) - 1) & b2g7; /* b2g7 if same diagonal */
    line += (((rank + file) & 15) - 1) & h1b7; /* h1b7 if same antidiag */
    line *= btwn & -btwn; /* mul acts like shift by smaller square */
    return line & btwn;   /* return the bits on that line in-between */
    // @formatter:on
}

void initBetweenTable() {
    for (uint8_t a = 0; a < 64; ++a) {
        for (uint8_t b = 0; b < 64; ++b) {
            if (a == b) {
                continue;
            }

            uint8_t index = x88Diff(a, b);
            Bitboard between = generateBetween(a, b);
            between0x88Table[index] = Intrinsics::ror(between, a);
        }
    }
}



// This is an expensive method, only call during initialization.
Bitboard generateRayAttacks(Square square, int8_t fileDelta, int8_t rankDelta, Bitboard blockers) {
    Bitboard attacks;

    int32_t file = Square::file(square) + fileDelta;
    int32_t rank = Square::rank(square) + rankDelta;

    while (file >= 0 && file <= 7 && rank >= 0 && rank <= 7) {
        Square attack(file, rank);

        attacks.set(attack);

        if (blockers.get(attack)) {
            break;
        }

        file += fileDelta;
        rank += rankDelta;
    }

    return attacks;
}

void initDiagonalAttacks(uint32_t &attackOffset) {
    for (uint8_t square = 0; square < 64; ++square) {
        // Generate the occupied mask for the square.
        Bitboard occupiedMask;
        {
            // Don't need to go all the way to the edge of the board since those squares will not affect the attacks
            // regardless of if they are occupied or not.
            int32_t file = Square::file(square) - 1;
            int32_t rank = Square::rank(square) - 1;
            while (file >= 1 && rank >= 1) {
                occupiedMask.set(Square(file--, rank--));
            }

            file = Square::file(square) + 1;
            rank = Square::rank(square) - 1;
            while (file < 7 && rank >= 1) {
                occupiedMask.set(Square(file++, rank--));
            }

            file = Square::file(square) - 1;
            rank = Square::rank(square) + 1;
            while (file >= 1 && rank < 7) {
                occupiedMask.set(Square(file--, rank++));
            }

            file = Square::file(square) + 1;
            rank = Square::rank(square) + 1;
            while (file < 7 && rank < 7) {
                occupiedMask.set(Square(file++, rank++));
            }
        }

        // Generate the attack mask for the square.
        Bitboard attackMask;
        attackMask |= generateRayAttacks(square, 1, 1, 0);
        attackMask |= generateRayAttacks(square, -1, 1, 0);
        attackMask |= generateRayAttacks(square, 1, -1, 0);
        attackMask |= generateRayAttacks(square, -1, -1, 0);

        // Generate all attacks for the square.
        assert(occupiedMask.count() <= 16 && "Too many occupied squares.");
        uint16_t attackCount = (1 << occupiedMask.count());

        for (uint16_t index = 0; index < attackCount; ++index) {
            Bitboard blockers = Intrinsics::pdep(index, occupiedMask);

            Bitboard attacks;

            attacks |= generateRayAttacks(square, 1, 1, blockers);
            attacks |= generateRayAttacks(square, -1, 1, blockers);
            attacks |= generateRayAttacks(square, 1, -1, blockers);
            attacks |= generateRayAttacks(square, -1, -1, blockers);

            uint64_t attackBits = Intrinsics::pext(attacks, attackMask);
            assert(attackBits <= UINT16_MAX && "Attack bits are too large.");
            slidingAttackTable[attackOffset + index] = static_cast<uint16_t>(attackBits);
        }

        diagonalPextTable[square] = { occupiedMask, attackMask, attackOffset };
        attackOffset += attackCount;
    }
}

void initOrthogonalAttacks(uint32_t &attackOffset) {
    for (uint8_t square = 0; square < 64; ++square) {
        // Generate the occupied mask for the square.
        Bitboard occupiedMask;
        {
            uint8_t file = Square::file(square);
            uint8_t rank = Square::rank(square);

            // Don't need to go all the way to the edge of the board since those squares will not affect the attacks
            // regardless of if they are occupied or not.
            for (uint8_t i = 1; i < file; ++i) {
                occupiedMask.set(Square(i, rank));
            }
            for (uint8_t i = file + 1; i < 7; ++i) {
                occupiedMask.set(Square(i, rank));
            }
            for (uint8_t i = 1; i < rank; ++i) {
                occupiedMask.set(Square(file, i));
            }
            for (uint8_t i = rank + 1; i < 7; ++i) {
                occupiedMask.set(Square(file, i));
            }
        }

        // Generate the attack mask for the square.
        Bitboard attackMask;
        attackMask |= generateRayAttacks(square, 1, 0, 0);
        attackMask |= generateRayAttacks(square, -1, 0, 0);
        attackMask |= generateRayAttacks(square, 0, 1, 0);
        attackMask |= generateRayAttacks(square, 0, -1, 0);

        // Generate all attacks for the square.
        assert(occupiedMask.count() <= 16 && "Too many occupied squares.");
        uint16_t attackCount = (1 << occupiedMask.count());

        for (uint16_t index = 0; index < attackCount; ++index) {
            Bitboard blockers = Intrinsics::pdep(index, occupiedMask);

            Bitboard attacks;

            attacks |= generateRayAttacks(square, 1, 0, blockers);
            attacks |= generateRayAttacks(square, -1, 0, blockers);
            attacks |= generateRayAttacks(square, 0, 1, blockers);
            attacks |= generateRayAttacks(square, 0, -1, blockers);

            uint64_t attackBits = Intrinsics::pext(attacks, attackMask);
            assert(attackBits <= UINT16_MAX && "Attack bits are too large.");
            slidingAttackTable[attackOffset + index] = static_cast<uint16_t>(attackBits);
        }

        orthogonalPextTable[square] = { occupiedMask, attackMask, attackOffset };
        attackOffset += attackCount;
    }
}

void initPawnAttacks() {
    pawnAttackTable.white().fill(0);
    for (uint8_t index = 0; index < 64; ++index) {
        Bitboard attacks;

        uint8_t file = Square::file(index);
        uint8_t rank = Square::rank(index);

        if (file > 0 && rank < 7) attacks.set(index + 7);
        if (file < 7 && rank < 7) attacks.set(index + 9);

        pawnAttackTable.white()[index] = attacks;
    }

    pawnAttackTable.black().fill(0);
    for (uint8_t index = 0; index < 64; ++index) {
        Bitboard attacks;

        uint8_t file = Square::file(index);
        uint8_t rank = Square::rank(index);

        if (file > 0 && rank > 0) attacks.set(index - 9);
        if (file < 7 && rank > 0) attacks.set(index - 7);

        pawnAttackTable.black()[index] = attacks;
    }
}

void initKnightAttacks() {
    knightAttackTable.fill(0);
    for (uint8_t index = 0; index < 64; ++index) {
        Bitboard attacks;

        uint8_t file = Square::file(index);
        uint8_t rank = Square::rank(index);

        if (file >= 2 && rank >= 1) attacks.set(index - 10);
        if (file >= 1 && rank >= 2) attacks.set(index - 17);
        if (file <= 6 && rank >= 2) attacks.set(index - 15);
        if (file <= 5 && rank >= 1) attacks.set(index - 6);
        if (file <= 5 && rank <= 6) attacks.set(index + 10);
        if (file <= 6 && rank <= 5) attacks.set(index + 17);
        if (file >= 1 && rank <= 5) attacks.set(index + 15);
        if (file >= 2 && rank <= 6) attacks.set(index + 6);

        knightAttackTable[index] = attacks;
    }
}

void initKingAttacks() {
    kingAttackTable.fill(0);
    for (uint8_t index = 0; index < 64; ++index) {
        Bitboard attacks;

        uint8_t file = Square::file(index);
        uint8_t rank = Square::rank(index);

        if (file >= 1 && rank >= 1) attacks.set(index - 9);
        if (file >= 1) attacks.set(index - 1);
        if (file >= 1 && rank <= 6) attacks.set(index + 7);
        if (rank >= 1) attacks.set(index - 8);
        if (rank <= 6) attacks.set(index + 8);
        if (file <= 6 && rank >= 1) attacks.set(index - 7);
        if (file <= 6) attacks.set(index + 1);
        if (file <= 6 && rank <= 6) attacks.set(index + 9);

        kingAttackTable[index] = attacks;
    }
}

} // namespace

void Bitboards::init() {
    initBetweenTable();

    uint32_t attackOffset = 0;
    initDiagonalAttacks(attackOffset);
    initOrthogonalAttacks(attackOffset);
    assert(attackOffset == slidingAttackTable.size() && "Sliding attack table size is incorrect.");

    initPawnAttacks();
    initKnightAttacks();
    initKingAttacks();
}



Bitboard Bitboards::between(Square a, Square b) {
    return Intrinsics::rol(between0x88Table[x88Diff(a, b)], a);
}



template<Color Side>
Bitboard Bitboards::pawnAttacks(Square square) {
    return pawnAttackTable[Side][square];
}

template Bitboard Bitboards::pawnAttacks<Color::White>(Square);
template Bitboard Bitboards::pawnAttacks<Color::Black>(Square);

Bitboard Bitboards::knightAttacks(Square square) {
    return knightAttackTable[square];
}

Bitboard Bitboards::bishopAttacks(Square square, Bitboard occupied) {
    const PextTableEntry &entry = diagonalPextTable[square];

    uint16_t attackBits = slidingAttackTable[entry.offset + Intrinsics::pext(occupied, entry.occupiedMask)];
    return Intrinsics::pdep(attackBits, entry.attackMask);
}

Bitboard Bitboards::rookAttacks(Square square, Bitboard occupied) {
    const PextTableEntry &entry = orthogonalPextTable[square];

    uint16_t attackBits = slidingAttackTable[entry.offset + Intrinsics::pext(occupied, entry.occupiedMask)];
    return Intrinsics::pdep(attackBits, entry.attackMask);
}

Bitboard Bitboards::queenAttacks(Square square, Bitboard occupied) {
    return bishopAttacks(square, occupied) | rookAttacks(square, occupied);
}

Bitboard Bitboards::kingAttacks(Square square) {
    return kingAttackTable[square];
}



Bitboard Bitboards::bishopAttacksOnEmpty(Square square) {
    return diagonalPextTable[square].attackMask;
}

Bitboard Bitboards::rookAttacksOnEmpty(Square square) {
    return orthogonalPextTable[square].attackMask;
}

Bitboard Bitboards::queenAttacksOnEmpty(Square square) {
    return bishopAttacksOnEmpty(square) | rookAttacksOnEmpty(square);
}



template<Color Side>
Bitboard Bitboards::allPawnAttacks(Bitboard pawns) {
    // Pawns are special since we can just shift them to get all the attacks in one go, instead of having to iterate
    // over each pawn and generate the attacks individually.

    // Pawns not on the A file can attack to the left, and pawns not on the H file can attack to the right (opposite
    // for black).
    constexpr Bitboard NonAFile = ~Bitboards::FileA;
    constexpr Bitboard NonHFile = ~Bitboards::FileH;

    if constexpr (Side == Color::White) {
        return ((pawns & NonAFile) << 7) | ((pawns & NonHFile) << 9);
    } else {
        return ((pawns & NonAFile) >> 9) | ((pawns & NonHFile) >> 7);
    }
}

template Bitboard Bitboards::allPawnAttacks<Color::White>(Bitboard);
template Bitboard Bitboards::allPawnAttacks<Color::Black>(Bitboard);

Bitboard Bitboards::allKnightAttacks(Bitboard knights) {
    Bitboard attacks;
    for (Square knight : knights) {
        attacks |= Bitboards::knightAttacks(knight);
    }
    return attacks;
}

Bitboard Bitboards::allBishopAttacks(Bitboard bishops, Bitboard occupied) {
    Bitboard attacks;
    for (Square bishop : bishops) {
        attacks |= Bitboards::bishopAttacks(bishop, occupied);
    }
    return attacks;
}

Bitboard Bitboards::allRookAttacks(Bitboard rooks, Bitboard occupied) {
    Bitboard attacks;
    for (Square rook : rooks) {
        attacks |= Bitboards::rookAttacks(rook, occupied);
    }
    return attacks;
}

Bitboard Bitboards::allQueenAttacks(Bitboard queens, Bitboard occupied) {
    Bitboard attacks;
    for (Square queen : queens) {
        attacks |= Bitboards::queenAttacks(queen, occupied);
    }
    return attacks;
}

template<Color Side>
Bitboard Bitboards::allAttacks(const Board &board, Bitboard occupied) {
    return allPawnAttacks<Side>(board.bitboard(Piece::pawn(Side)))
        | allKnightAttacks(board.bitboard(Piece::knight(Side)))
        | allBishopAttacks(board.bitboard(Piece::bishop(Side)), occupied)
        | allRookAttacks(board.bitboard(Piece::rook(Side)), occupied)
        | allQueenAttacks(board.bitboard(Piece::queen(Side)), occupied)
        | kingAttacks(board.king(Side));
}

template Bitboard Bitboards::allAttacks<Color::White>(const Board &, Bitboard);
template Bitboard Bitboards::allAttacks<Color::Black>(const Board &, Bitboard);

} // namespace FKTB
