#include "color.h"

namespace FKTB {

std::string ColorNamespace::debugName(Color color) {
    // @formatter:off
    switch (color) {
        case Color::White:  return "White";
        case Color::Black:  return "Black";
        default:            return "Unknown";
    }
    // @formatter:on
}

} // namespace FKTB
