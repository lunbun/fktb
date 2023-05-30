#include "bitboard.h"

#include <string>

#include "engine/inline.h"

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



AttackTable Bitboards::northAttacks;
AttackTable Bitboards::northEastAttacks;
AttackTable Bitboards::eastAttacks;
AttackTable Bitboards::southEastAttacks;
AttackTable Bitboards::southAttacks;
AttackTable Bitboards::southWestAttacks;
AttackTable Bitboards::westAttacks;
AttackTable Bitboards::northWestAttacks;

ColorMap<AttackTable> Bitboards::pawnAttacks;
AttackTable Bitboards::knightAttacks;
AttackTable Bitboards::kingAttacks;

void initRayAttacks(AttackTable &table, int8_t fileOffset, int8_t rankOffset) {
    table.fill(0);

    for (uint8_t index = 0; index < 64; ++index) {
        Bitboard attacks;

        auto file = static_cast<int8_t>(Square::file(index) + fileOffset);
        auto rank = static_cast<int8_t>(Square::rank(index) + rankOffset);

        while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
            attacks.set(Square(file, rank));

            file += fileOffset;
            rank += rankOffset;
        }

        table[index] = attacks;
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

    initRayAttacks(northAttacks, 0, 1);
    initRayAttacks(northEastAttacks, 1, 1);
    initRayAttacks(eastAttacks, 1, 0);
    initRayAttacks(southEastAttacks, 1, -1);
    initRayAttacks(southAttacks, 0, -1);
    initRayAttacks(southWestAttacks, -1, -1);
    initRayAttacks(westAttacks, -1, 0);
    initRayAttacks(northWestAttacks, -1, 1);

    initPawnAttacks();
    initKnightAttacks();
    initKingAttacks();
}



// The approach described in the following article is how we generate attacks for sliding pieces:
// https://www.chessprogramming.org/Classical_Approach
template<const AttackTable &Table>
INLINE Bitboard generatePositiveAttacks(Square square, Bitboard occupied) {
    Bitboard attacks = Table[square];
    Bitboard blockers = (attacks & occupied) | 0x8000000000000000ULL;
    return attacks & ~Table[blockers.bsf()];
}

template<const AttackTable &Table>
INLINE Bitboard generateNegativeAttacks(Square square, Bitboard occupied) {
    Bitboard attacks = Table[square];
    Bitboard blockers = (attacks & occupied) | 1ULL;
    return attacks & ~Table[blockers.bsr()];
}

Bitboard Bitboards::orthogonal(Square square, Bitboard occupied) {
    return generatePositiveAttacks<Bitboards::northAttacks>(square, occupied)
        | generateNegativeAttacks<Bitboards::southAttacks>(square, occupied)
        | generatePositiveAttacks<Bitboards::eastAttacks>(square, occupied)
        | generateNegativeAttacks<Bitboards::westAttacks>(square, occupied);
}

Bitboard Bitboards::diagonal(Square square, Bitboard occupied) {
    return generatePositiveAttacks<Bitboards::northEastAttacks>(square, occupied)
        | generateNegativeAttacks<Bitboards::southEastAttacks>(square, occupied)
        | generatePositiveAttacks<Bitboards::northWestAttacks>(square, occupied)
        | generateNegativeAttacks<Bitboards::southWestAttacks>(square, occupied);
}



template<Color Side>
Bitboard Bitboards::allPawn(Bitboard pawns) {
    Bitboard attacks;
    while (pawns) {
        attacks = attacks | Bitboards::pawn<Side>(pawns.bsfReset());
    }
    return attacks;
}

template Bitboard Bitboards::allPawn<Color::White>(Bitboard pawns);
template Bitboard Bitboards::allPawn<Color::Black>(Bitboard pawns);

Bitboard Bitboards::allKnight(Bitboard knights) {
    Bitboard attacks;
    while (knights) {
        attacks = attacks | Bitboards::knight(knights.bsfReset());
    }
    return attacks;
}

Bitboard Bitboards::allBishop(Bitboard bishops, Bitboard occupied) {
    Bitboard attacks;
    while (bishops) {
        attacks = attacks | Bitboards::bishop(bishops.bsfReset(), occupied);
    }
    return attacks;
}

Bitboard Bitboards::allRook(Bitboard rooks, Bitboard occupied) {
    Bitboard attacks;
    while (rooks) {
        attacks = attacks | Bitboards::rook(rooks.bsfReset(), occupied);
    }
    return attacks;
}

Bitboard Bitboards::allQueen(Bitboard queens, Bitboard occupied) {
    Bitboard attacks;
    while (queens) {
        attacks = attacks | Bitboards::queen(queens.bsfReset(), occupied);
    }
    return attacks;
}

Bitboard Bitboards::allKing(Bitboard kings) {
    Bitboard attacks;
    while (kings) {
        attacks = attacks | Bitboards::king(kings.bsfReset());
    }
    return attacks;
}
