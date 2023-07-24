#include "evaluation.h"

#include <cstdint>

#include "game_phase.h"
#include "engine/inline.h"
#include "engine/board/square.h"
#include "engine/board/bitboard.h"

namespace FKTB {

namespace {

template<Color Side>
INLINE int32_t evaluatePawnShield(const Board &board);

// Evaluates the pawn shield for the given side, with a bitboard mask for the first pawn shield (i.e. the pawns on the starting
// rank in front of the king).
template<Color Side, uint64_t Mask>
INLINE int32_t evaluatePawnShieldWithMask(const Board &board) {
    constexpr Color Enemy = ~Side;
    constexpr Bitboard Mask2 = Bitboard(Mask).shiftForward<Side>(1);

    int32_t score = 0;

    Bitboard pawns = board.bitboard(Piece::pawn(Side));
    Bitboard pawnShield1 = pawns & Mask;
    Bitboard pawnShield2 = pawns & Mask2;

    // Remove doubled pawns from the second pawn shield.
    pawnShield2 &= ~pawnShield1.shiftForward<Side>(1);

    Bitboard missingShields = ~(pawnShield1 | pawnShield2.shiftBackward<Side>(1)) & Mask;

    uint8_t pawnShield1Count = pawnShield1.count();
    uint8_t pawnShield2Count = pawnShield2.count();
    uint8_t missingShieldCount = missingShields.count();

    score += 10 * pawnShield1Count;
    score += 8 * pawnShield2Count;
    score -= 8 * missingShieldCount;

    Bitboard enemyPawns = board.bitboard(Piece::pawn(Enemy));
    Bitboard enemyRooks = board.bitboard(Piece::rook(Enemy));
    for (Square missingShield : missingShields) {
        Bitboard file = Bitboards::file(missingShield.file());

        // Penalty for enemy having an open file on the same file as a missing pawn shield.
        if (!(enemyPawns & file)) {
            score -= 8;

            // Even larger penalty if there is a rook on the file.
            if (enemyRooks & file) {
                score -= 8;
            }
        }
    }

    return score;
}

template<>
INLINE int32_t evaluatePawnShield<Color::White>(const Board &board) {
    Square king = board.king(Color::White);

    // Only evaluate pawn shield if king is on first two ranks.
    if (king.rank() > 1) {
        return 0;
    }

    if (king.file() >= 5) { // King is on the king-side.
        return evaluatePawnShieldWithMask<Color::White, Bitboards::F2 | Bitboards::G2 | Bitboards::H2>(board);
    } else if (king.file() <= 2) { // King is on the queen-side.
        return evaluatePawnShieldWithMask<Color::White, Bitboards::A2 | Bitboards::B2 | Bitboards::C2>(board);
    } else {
        return 0;
    }
}

template<>
INLINE int32_t evaluatePawnShield<Color::Black>(const Board &board) {
    Square king = board.king(Color::Black);

    // Only evaluate pawn shield if king is on first two ranks.
    if (king.rank() < 6) {
        return 0;
    }

    if (king.file() >= 5) { // King is on the king-side.
        return evaluatePawnShieldWithMask<Color::Black, Bitboards::F7 | Bitboards::G7 | Bitboards::H7>(board);
    } else if (king.file() <= 2) { // King is on the queen-side.
        return evaluatePawnShieldWithMask<Color::Black, Bitboards::A7 | Bitboards::B7 | Bitboards::C7>(board);
    } else {
        return 0;
    }
}



struct KingAttackSquareData {
    uint16_t attackerCount;
    uint16_t attackerWeight;
};

struct KingAttack {
    Bitboard kingZone;
    SquareMap<KingAttackSquareData> kingZoneAttacks;
    uint16_t totalAttackerCount;

    explicit KingAttack(Bitboard kingZone) : kingZone(kingZone), kingZoneAttacks(), totalAttackerCount(0) { }
};

// The king zone is the important squares that must be protected for the king to be safe.
template<Color Side>
INLINE Bitboard calculateKingZone(Square king, Bitboard occupied) {
    // Squares the king can reach.
    Bitboard kingZone = Bitboards::kingAttacks(king);

    // Extend the king zone two ranks towards the enemy side.
    kingZone |= kingZone.shiftForward<Side>(1);
    kingZone |= kingZone.shiftForward<Side>(1);

    // Only consider squares that would put the king in check.
    kingZone &= Bitboards::queenAttacks(king, occupied);

    // Include the king square.
    kingZone |= (1ULL << king);

    return kingZone;
}

// Adds the king zone attacks to the king attack if there are any.
INLINE void maybeAddKingZoneAttacksToKingAttack(KingAttack &attack, Bitboard attacks, int32_t material) {
    Bitboard kingZoneAttacks = attacks & attack.kingZone;

    // If there are no king zone attacks, then we don't need to add anything.
    if (!kingZoneAttacks) {
        return;
    }

    attack.totalAttackerCount++;

    for (Square attackedSquare : kingZoneAttacks) {
        attack.kingZoneAttacks[attackedSquare].attackerCount++;
        attack.kingZoneAttacks[attackedSquare].attackerWeight += material;
    }
}

template<Color Side>
INLINE void addAllKnightAttacksToKingAttack(KingAttack &attack, const Board &board) {
    for (Square knight : board.bitboard(Piece::knight(Side))) {
        maybeAddKingZoneAttacksToKingAttack(attack, Bitboards::knightAttacks(knight), PieceMaterial::Knight);
    }
}

template<Color Side, PieceType Slider>
INLINE void addAllSliderAttacksToKingAttack(KingAttack &attack, Bitboard queens, Bitboard occupied, const Board &board) {
    Bitboard slidersOfType = board.bitboard({ Side, Slider });

    // Queens are both rooks and bishops.
    Bitboard allSlidersOfType = slidersOfType | queens;

    // Remove sliders of the same type from the occupied bitboard, so that when we generate attacks, other sliders of the same
    // type can x-ray through fellow sliders.
    Bitboard occupiedXRay = occupied ^ allSlidersOfType;

    // TODO: If a slider is creating a battery towards a king zone with a fellow slider of the same type, then the attack is more
    //  dangerous.
    for (Square slider : slidersOfType) {
        maybeAddKingZoneAttacksToKingAttack(attack, Bitboards::sliderAttacks(Slider, slider, occupiedXRay),
            PieceMaterial::value(Slider));
    }
    for (Square queen : queens) {
        maybeAddKingZoneAttacksToKingAttack(attack, Bitboards::sliderAttacks(Slider, queen, occupiedXRay), PieceMaterial::Queen);
    }
}

// Evaluates the enemy's attack on our king. Returns a negative value in centipawns if the enemy is attacking our king. The more
// dangerous the attack is, the larger the negative value will be.
template<Color Side>
INLINE int32_t evaluateKingAttack(const Board &board) {
    constexpr Color Enemy = ~Side;

    Bitboard occupied = board.occupied();

    Bitboard kingZone = calculateKingZone<Side>(board.king(Side), occupied);
    KingAttack attack(kingZone);

    // TODO: We might be able to skip the pawn shield evaluation if we instead penalize larger king zones.

    // Add the king zone attacks from all enemy pieces.
    // TODO: If a friendly piece is defending the attacked square, then the attack is not as dangerous.
    Bitboard queens = board.bitboard(Piece::queen(Enemy));
    addAllKnightAttacksToKingAttack<Enemy>(attack, board);
    addAllSliderAttacksToKingAttack<Enemy, PieceType::Bishop>(attack, queens, occupied, board);
    addAllSliderAttacksToKingAttack<Enemy, PieceType::Rook>(attack, queens, occupied, board);

    // One piece cannot checkmate on its own, so there is no attack if there is one or fewer attackers.
    if (attack.totalAttackerCount <= 1) {
        return 0;
    }

    int32_t score = 0;
    for (Square kingZoneSquare : kingZone) {
        const KingAttackSquareData &data = attack.kingZoneAttacks[kingZoneSquare];

        // The more pieces lined up on a square, the more dangerous it is. If the pieces are attacking separate squares, then it
        // is not as dangerous (but still dangerous).
        score -= data.attackerWeight * data.attackerWeight * data.attackerCount / 50000;
    }

    return score;
}



// Evaluates the king safety for the given side.
template<Color Side>
INLINE int32_t evaluateKingSafety(const Board &board) {
    int32_t score = 0;

    // Penalty for being in the center.
    Square king = board.king(Side);
    if ((3 <= king.file()) && (king.file() <= 4)) {
        score -= 40;
    }

    score += evaluatePawnShield<Side>(board);
    score += evaluateKingAttack<Side>(board);

    return score;
}



// Stage 1 of lazy evaluation (fast evaluation)
template<GamePhase Phase, Color Side>
INLINE int32_t evaluateFastForSide(const Board &board) {
    int32_t score = 0;

    // Material
    score += board.material(Side);

    // Bonus for having a bishop pair
    score += PieceMaterial::BishopPair * (board.bitboard(Piece::bishop(Side)).count() >= 2);

    // Piece square tables
    score += board.pieceSquareEval(Phase, Side);

    return score;
}

// Stage 1 of lazy evaluation for both sides, subtracting the evaluation for the other side.
template<GamePhase Phase, Color Side>
int32_t evaluateFast(const Board &board) {
    int32_t score = evaluateFastForSide<Phase, Side>(board) - evaluateFastForSide<Phase, ~Side>(board);

    // Tempo bonus
    if constexpr (Phase == GamePhase::Opening) {
        constexpr int32_t TempoBonus = 20;
        score += TempoBonus;
    }

    return score;
}

// Stage 2 of lazy evaluation (slower evaluation, only done if the fast evaluation does not cause a cutoff)
template<GamePhase Phase, Color Side>
INLINE int32_t evaluateCompleteForSide(const Board &board) {
    int32_t score = 0;

    // King safety
    if constexpr (Phase == GamePhase::Opening) {
        score += evaluateKingSafety<Side>(board);
    }

    return score;
}

// Stage 2 of lazy evaluation for both sides, subtracting the evaluation for the other side.
template<GamePhase Phase, Color Side>
int32_t evaluateComplete(const Board &board) {
    return evaluateCompleteForSide<Phase, Side>(board) - evaluateCompleteForSide<Phase, ~Side>(board);
}

} // namespace

// Evaluates the board for the given side, subtracting the evaluation for the other side. Interpolates between the opening and end
// game phases.
template<Color Side>
int32_t Evaluation::evaluate(const Board &board, int32_t alpha, int32_t beta) {
    constexpr int32_t LazyEvalMargin = 150;

    uint16_t phase = TaperedEval::calculateContinuousPhase(board);

    if (phase == GamePhase::Opening) { // Strictly opening

        int32_t score = evaluateFast<GamePhase::Opening, Side>(board);

        // Check if we can prune the evaluation.
        if (score - LazyEvalMargin > beta || score + LazyEvalMargin < alpha) {
            return score;
        }

        return score + evaluateComplete<GamePhase::Opening, Side>(board);

    } else if (phase == GamePhase::End) { // Strictly end game

        int32_t score = evaluateFast<GamePhase::End, Side>(board);

        // Check if we can prune the evaluation.
        if (score - LazyEvalMargin > beta || score + LazyEvalMargin < alpha) {
            return score;
        }

        return score + evaluateComplete<GamePhase::End, Side>(board);

    } else { // Interpolate between opening and end game

        int32_t openingScore = evaluateFast<GamePhase::Opening, Side>(board);
        int32_t endScore = evaluateFast<GamePhase::End, Side>(board);
        int32_t score = TaperedEval::interpolate(openingScore, endScore, phase);

        // Check if we can prune the evaluation.
        if (score - LazyEvalMargin > beta || score + LazyEvalMargin < alpha) {
            return score;
        }

        openingScore += evaluateComplete<GamePhase::Opening, Side>(board);
        endScore += evaluateComplete<GamePhase::End, Side>(board);
        return TaperedEval::interpolate(openingScore, endScore, phase);

    }
}

template int32_t Evaluation::evaluate<Color::White>(const Board &, int32_t, int32_t);
template int32_t Evaluation::evaluate<Color::Black>(const Board &, int32_t, int32_t);

} // namespace FKTB
