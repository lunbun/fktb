#include "bitboard.h"

#include <string>
#include <array>

#include "engine/inline.h"
#include "engine/intrinsics.h"

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



SquareMap<Bitboard> Bitboards::diagonalMasks;
SquareMap<Bitboard> Bitboards::orthogonalMasks;
SquareMap<std::array<Bitboard, 512>> Bitboards::diagonalAttacks;
SquareMap<std::array<Bitboard, 4096>> Bitboards::orthogonalAttacks;

ColorMap<SquareMap<Bitboard>> Bitboards::pawnAttacks;
SquareMap<Bitboard> Bitboards::knightAttacks;
SquareMap<Bitboard> Bitboards::kingAttacks;

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

void initDiagonalAttacks() {
    for (uint8_t square = 0; square < 64; ++square) {
        // Generate the mask for the square.
        Bitboard mask;
        {
            // Don't need to go all the way to the edge of the board since those squares will not affect the attacks
            // regardless of if they are occupied or not.
            int32_t file = Square::file(square) - 1;
            int32_t rank = Square::rank(square) - 1;
            while (file >= 1 && rank >= 1) {
                mask.set(Square(file--, rank--));
            }

            file = Square::file(square) + 1;
            rank = Square::rank(square) - 1;
            while (file < 7 && rank >= 1) {
                mask.set(Square(file++, rank--));
            }

            file = Square::file(square) - 1;
            rank = Square::rank(square) + 1;
            while (file >= 1 && rank < 7) {
                mask.set(Square(file--, rank++));
            }

            file = Square::file(square) + 1;
            rank = Square::rank(square) + 1;
            while (file < 7 && rank < 7) {
                mask.set(Square(file++, rank++));
            }

            Bitboards::diagonalMasks[square] = mask;
        }

        // Generate all attacks for the square.
        for (uint64_t index = 0; index < 512; ++index) {
            Bitboard blockers = Intrinsics::pdep(index, mask);

            Bitboard attacks;

            attacks |= generateRayAttacks(square, 1, 1, blockers);
            attacks |= generateRayAttacks(square, -1, 1, blockers);
            attacks |= generateRayAttacks(square, 1, -1, blockers);
            attacks |= generateRayAttacks(square, -1, -1, blockers);

            Bitboards::diagonalAttacks[square][index] = attacks;
        }
    }
}

void initOrthogonalAttacks() {
    for (uint8_t square = 0; square < 64; ++square) {
        // Generate the mask for the square.
        Bitboard mask;
        {
            uint8_t file = Square::file(square);
            uint8_t rank = Square::rank(square);

            // Don't need to go all the way to the edge of the board since those squares will not affect the attacks
            // regardless of if they are occupied or not.
            for (uint8_t i = 1; i < file; ++i) {
                mask.set(Square(i, rank));
            }
            for (uint8_t i = file + 1; i < 7; ++i) {
                mask.set(Square(i, rank));
            }
            for (uint8_t i = 1; i < rank; ++i) {
                mask.set(Square(file, i));
            }
            for (uint8_t i = rank + 1; i < 7; ++i) {
                mask.set(Square(file, i));
            }

            Bitboards::orthogonalMasks[square] = mask;
        }

        // Generate all attacks for the square.
        for (uint64_t index = 0; index < 4096; ++index) {
            Bitboard blockers = Intrinsics::pdep(index, mask);

            Bitboard attacks;

            attacks |= generateRayAttacks(square, 1, 0, blockers);
            attacks |= generateRayAttacks(square, -1, 0, blockers);
            attacks |= generateRayAttacks(square, 0, 1, blockers);
            attacks |= generateRayAttacks(square, 0, -1, blockers);

            Bitboards::orthogonalAttacks[square][index] = attacks;
        }
    }
}

void initPawnAttacks() {
    Bitboards::pawnAttacks.white().fill(0);
    for (uint8_t index = 0; index < 64; ++index) {
        Bitboard attacks;

        uint8_t file = Square::file(index);
        uint8_t rank = Square::rank(index);

        if (file > 0 && rank < 7) attacks.set(index + 7);
        if (file < 7 && rank < 7) attacks.set(index + 9);

        Bitboards::pawnAttacks.white()[index] = attacks;
    }

    Bitboards::pawnAttacks.black().fill(0);
    for (uint8_t index = 0; index < 64; ++index) {
        Bitboard attacks;

        uint8_t file = Square::file(index);
        uint8_t rank = Square::rank(index);

        if (file > 0 && rank > 0) attacks.set(index - 9);
        if (file < 7 && rank > 0) attacks.set(index - 7);

        Bitboards::pawnAttacks.black()[index] = attacks;
    }
}

void initKnightAttacks() {
    Bitboards::knightAttacks.fill(0);
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

        Bitboards::knightAttacks[index] = attacks;
    }
}

void initKingAttacks() {
    Bitboards::kingAttacks.fill(0);
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

        Bitboards::kingAttacks[index] = attacks;
    }
}

void Bitboards::maybeInit() {
    static bool isInitialized = false;

    if (isInitialized) {
        return;
    }

    isInitialized = true;

    initDiagonalAttacks();
    initOrthogonalAttacks();

    initPawnAttacks();
    initKnightAttacks();
    initKingAttacks();
}



template<Color Side>
Bitboard Bitboards::allPawn(Bitboard pawns) {
    Bitboard attacks;
    for (Square pawn : pawns) {
        attacks |= Bitboards::pawn<Side>(pawn);
    }
    return attacks;
}

template Bitboard Bitboards::allPawn<Color::White>(Bitboard pawns);
template Bitboard Bitboards::allPawn<Color::Black>(Bitboard pawns);

Bitboard Bitboards::allKnight(Bitboard knights) {
    Bitboard attacks;
    for (Square knight : knights) {
        attacks |= Bitboards::knight(knight);
    }
    return attacks;
}

Bitboard Bitboards::allBishop(Bitboard bishops, Bitboard occupied) {
    Bitboard attacks;
    for (Square bishop : bishops) {
        attacks |= Bitboards::bishop(bishop, occupied);
    }
    return attacks;
}

Bitboard Bitboards::allRook(Bitboard rooks, Bitboard occupied) {
    Bitboard attacks;
    for (Square rook : rooks) {
        attacks |= Bitboards::rook(rook, occupied);
    }
    return attacks;
}

Bitboard Bitboards::allQueen(Bitboard queens, Bitboard occupied) {
    Bitboard attacks;
    for (Square queen : queens) {
        attacks |= Bitboards::queen(queen, occupied);
    }
    return attacks;
}
