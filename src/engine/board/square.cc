#include "square.h"

std::string Square::uci() const {
    std::string name;

    name += static_cast<char>(this->file() + 'a');
    name += static_cast<char>(this->rank() + '1');

    return name;
}

std::string Square::debugName() const {
    return this->uci();
}
