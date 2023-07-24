#pragma once

#include <cstdint>

#include "engine/inline.h"

namespace FKTB::Score {

constexpr static int32_t Draw = 0;

INLINE constexpr int32_t mateIn(uint16_t ply) { return -INT32_MAX + ply; }

INLINE constexpr bool isMate(int32_t score) {
    constexpr int32_t MateThreshold = Score::mateIn(INT16_MAX);
    return score <= MateThreshold || score >= -MateThreshold;
}

// Returns the number of plies until mate. Returns a positive number if opponent is getting mated, and a negative
// number if we are getting mated. Assumes the score is a mate score, otherwise returns garbage.
INLINE constexpr int32_t matePlies(int32_t score) {
    if (score < 0) { // We are getting mated
        return -(score + INT32_MAX);
    } else { // Opponent is getting mated
        return (-score + INT32_MAX);
    }
}

} // namespace FKTB::Score
