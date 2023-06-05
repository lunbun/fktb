#include "square.h"

#include <stdexcept>

Square Square::fromUci(const std::string &uci) {
    if (uci.length() != 2) {
        throw std::invalid_argument("Square string must be 2 characters long");
    }

    auto file = static_cast<uint8_t>(uci[0] - 'a');
    auto rank = static_cast<uint8_t>(uci[1] - '1');

    return { file, rank };
}

Square Square::fromFen(const std::string &fen) {
    return Square::fromUci(fen);
}



std::string Square::uci() const {
    std::string name;

    name += static_cast<char>(this->file() + 'a');
    name += static_cast<char>(this->rank() + '1');

    return name;
}

std::string Square::fen() const {
    return this->uci();
}

std::string Square::debugName() const {
    return this->uci();
}
