#pragma once

#include <cstdint>
#include <array>

#include "engine/inline.h"

// @formatter:off
namespace GamePhaseNamespace {
    enum GamePhase : uint16_t {
        // These values are also the bounds of the result of calculateContinuousPhase.
        Opening = 0,
        End     = 256
    };
}
using GamePhase = GamePhaseNamespace::GamePhase;
// @formatter:on

// Map of discrete game phases to values.
template<typename T>
class GamePhaseMap {
public:
    constexpr GamePhaseMap() : values_{ } { }
    constexpr GamePhaseMap(const T &opening, const T &end) : values_{ opening, end } { }

    [[nodiscard]] INLINE constexpr T &operator[](GamePhase phase) { return this->values_[phase >> 8]; }
    [[nodiscard]] INLINE constexpr const T &operator[](GamePhase phase) const { return this->values_[phase >> 8]; }

    [[nodiscard]] INLINE constexpr T &opening() { return this->values_[0]; }
    [[nodiscard]] INLINE constexpr T &end() { return this->values_[1]; }
    [[nodiscard]] INLINE constexpr const T &opening() const { return this->values_[0]; }
    [[nodiscard]] INLINE constexpr const T &end() const { return this->values_[1]; }

private:
    std::array<T, 2> values_;
};

class Board;

// Code mostly taken from https://www.chessprogramming.org/Tapered_Eval
namespace TaperedEval {
    // Returns a value between 0 and 256, where 0 is the opening and 256 is the end game.
    [[nodiscard]] uint16_t calculateContinuousPhase(const Board &board);

    // Interpolates the evaluation between the opening and end game based on the continuous phase.
    [[nodiscard]] INLINE constexpr int32_t interpolate(int32_t opening, int32_t endGame, uint8_t phase) {
        return ((opening * (256 - phase)) + (endGame * phase)) / 256;
    }
}
