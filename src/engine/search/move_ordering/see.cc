#include "see.h"

#include <cstdint>
#include <cassert>
#include <array>

#include "engine/inline.h"
#include "engine/intrinsics.h"
#include "engine/board/piece.h"
#include "engine/board/bitboard.h"

namespace FKTB {

namespace {

// We need a separate piece material table for SEE because SEE does not check for move legality, so we need the king to have an
// insanely high material value to discourage capturing with the king if it would leave it in check.
namespace SeeMaterial {

constexpr int32_t King = 200000;
constexpr PieceTypeMap<int32_t> Values = {
    PieceMaterial::Pawn,
    PieceMaterial::Knight,
    PieceMaterial::Bishop,
    PieceMaterial::Rook,
    PieceMaterial::Queen,
    King
};

[[nodiscard]] INLINE constexpr int32_t value(PieceType type) {
    assert(type != PieceType::Empty);
    return Values[type];
}

[[nodiscard]] INLINE constexpr int32_t value(Piece piece) {
    return value(piece.type());
}

} // namespace SeeMaterial

// Returns a bitboard of all attackers of a square of either color.
INLINE Bitboard findAllAttackers(Square square, Bitboard diagonalSliders, Bitboard orthogonalSliders, Bitboard occupied,
    const Board &board) {
    return (Bitboards::pawnAttacks<Color::White>(square) & board.bitboard(Piece::pawn(Color::Black)))
        | (Bitboards::pawnAttacks<Color::Black>(square) & board.bitboard(Piece::pawn(Color::White)))
        | (Bitboards::knightAttacks(square) & board.composite(PieceType::Knight))
        | (Bitboards::bishopAttacks(square, occupied) & diagonalSliders)
        | (Bitboards::rookAttacks(square, occupied) & orthogonalSliders);
}



struct LvaResult {
    INLINE constexpr static LvaResult invalid() { return { 0ULL, 0 }; }

    Bitboard bitboard;
    int32_t material;

    INLINE constexpr LvaResult(Bitboard bitboard, int32_t material) : bitboard(bitboard), material(material) { }

    [[nodiscard]] INLINE constexpr bool isValid() const { return bitboard != 0ULL; }
};

INLINE LvaResult findLeastValuableAttacker(Color side, Bitboard attackers, const Board &board) {
    for (PieceType type = PieceType::Pawn; type <= PieceType::Queen; type = static_cast<PieceType>(type + 1)) {
        Bitboard attackersOfType = attackers & board.bitboard({ side, type });

        if (attackersOfType) {
            return { Intrinsics::blsi(attackersOfType), SeeMaterial::value(type) };
        }
    }

    if (attackers.get(board.king(side))) {
        return { 1ULL << board.king(side), SeeMaterial::King };
    }

    return LvaResult::invalid();
}

} // namespace



// Based on https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
template<Color Defender>
int32_t See::evaluate(Square square, const Board &board) {
    constexpr uint8_t MaxDepth = 32;

    std::array<int32_t, MaxDepth> scores = { 0 };

    uint16_t depth = 1;
    Color side = ~Defender;

    Bitboard diagonalSliders = board.composite(PieceType::Bishop) | board.composite(PieceType::Queen);
    Bitboard orthogonalSliders = board.composite(PieceType::Rook) | board.composite(PieceType::Queen);

    // All pieces that can be x-rayed through
    Bitboard diagonalXRay = board.composite(PieceType::Pawn) | diagonalSliders;
    Bitboard orthogonalXRay = orthogonalSliders;

    Bitboard occupied = board.occupied();
    Bitboard attackers = findAllAttackers(square, diagonalSliders, orthogonalSliders, occupied, board);

    // The material value of the piece being captured
    int32_t captureMaterial = SeeMaterial::value(board.pieceAt(square));

    while (true) {
        // Find the least valuable attacker
        LvaResult attacker = findLeastValuableAttacker(side, attackers, board);

        if (!attacker.isValid()) {
            break;
        }

        // Add to the score
        scores[depth] = captureMaterial - scores[depth - 1];

        // Pruning
        if (std::max(-scores[depth - 1], scores[depth]) < 0) {
            break;
        }

        // Remove the attacker from the bitboards
        occupied ^= attacker.bitboard;
        attackers ^= attacker.bitboard;
        diagonalSliders &= ~attacker.bitboard;
        orthogonalSliders &= ~attacker.bitboard;

        // Need to update attackers if it is possible that the attacker was x-rayed
        if (attacker.bitboard & diagonalXRay) {
            attackers |= (Bitboards::bishopAttacks(square, occupied) & diagonalSliders);
        } else if (attacker.bitboard & orthogonalXRay) {
            attackers |= (Bitboards::rookAttacks(square, occupied) & orthogonalSliders);
        }

        // Update capture material
        captureMaterial = attacker.material;

        depth++;
        side = ~side;
    }

    // Traverse the "unary" tree and find the best score using negamax
    while (--depth) {
        scores[depth - 1] = -std::max(-scores[depth - 1], scores[depth]);
    }

    return scores[0];
}

template<Color Side>
int32_t See::evaluate(Move move, Board &board) {
    // We are only using the piece array and bitboards, so we can use MakeMoveType::BitboardsOnly
    MakeMoveInfo info = board.makeMove<MakeMoveType::BitboardsOnly>(move);

    int32_t score = 0;

    if (move.isCapture()) {
        score += SeeMaterial::value(info.captured);
    }

    score += evaluate<Side>(move.to(), board);

    board.unmakeMove<MakeMoveType::BitboardsOnly>(move, info);

    return score;
}



template int32_t See::evaluate<Color::White>(Square, const Board &);
template int32_t See::evaluate<Color::Black>(Square, const Board &);
template int32_t See::evaluate<Color::White>(Move, Board &);
template int32_t See::evaluate<Color::Black>(Move, Board &);

} // namespace FKTB
