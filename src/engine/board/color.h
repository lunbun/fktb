#pragma once

#include <cstdint>
#include <string>
#include <array>

#include "engine/inline.h"

namespace FKTB {

// @formatter:off
namespace ColorNamespace {
enum Color : uint8_t {
    White       = 0,
    Black       = 1
};

[[nodiscard]] std::string debugName(Color color);
} // namespace ColorNamespace
using Color = ColorNamespace::Color;
// @formatter:on

INLINE constexpr Color operator~(Color color) {
    return static_cast<Color>(color ^ 1);
}

template<typename T>
class ColorMap {
public:
    constexpr ColorMap() : values_{ } { }
    constexpr ColorMap(const T &white, const T &black) : values_{ white, black } { }

    [[nodiscard]] INLINE constexpr T &operator[](Color color) { return this->values_[color]; }
    [[nodiscard]] INLINE constexpr const T &operator[](Color color) const { return this->values_[color]; }

    [[nodiscard]] INLINE constexpr T &white() { return this->values_[0]; }
    [[nodiscard]] INLINE constexpr T &black() { return this->values_[1]; }
    [[nodiscard]] INLINE constexpr const T &white() const { return this->values_[0]; }
    [[nodiscard]] INLINE constexpr const T &black() const { return this->values_[1]; }

private:
    std::array<T, 2> values_;
};

} // namespace FKTB
