#include <iostream>

#include "engine/init.h"

#include "test.h"
#include "uci.h"

[[noreturn]] void uci() {
    std::string name = "FKTB 0.0.76";
#ifndef NDEBUG
    name += " Debug";
#endif // #ifndef NDEBUG
    FKTB::UCIHandler uci(name, "frogscats");

    uci.run();
}

int main() {
    FKTB::init();

    uci();
}
