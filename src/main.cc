#include <iostream>

#include "engine/init.h"

#include "test.h"
#include "uci.h"

[[noreturn]] void uci() {
    std::string name = "FKTB 0.0.65 move-streams";
#ifndef NDEBUG
    name += " Debug";
#endif // #ifndef NDEBUG
    UciHandler uci(name, "frogscats");

    uci.run();
}

int main() {
    EngineInit::init();

    // SPRT results:
    // 0.0.65 move-streams      vs 0.0.64               -14.6 +/- 16.2 Elo (loss, but error is large, so we can't be sure)

    uci();
}
