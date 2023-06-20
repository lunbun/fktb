#include <iostream>

#include "engine/init.h"

#include "test.h"
#include "uci.h"

[[noreturn]] void uci() {
    std::string name = "FKTB 0.0.64";
#ifndef NDEBUG
    name += " Debug";
#endif // #ifndef NDEBUG
    UciHandler uci(name, "frogscats");

    uci.run();
}

int main() {
    EngineInit::init();

    uci();
}
