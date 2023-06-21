#include <iostream>

#include "engine/init.h"

#include "test.h"
#include "uci.h"

[[noreturn]] void uci() {
    std::string name = "FKTB 0.0.66 move-streams";
#ifndef NDEBUG
    name += " Debug";
#endif // #ifndef NDEBUG
    UciHandler uci(name, "frogscats");

    uci.run();
}

int main() {
    EngineInit::init();

    // SPRT results:
    // 0.0.65 move-streams              vs  0.0.64                  test 1      -14.6 +/- 16.2 Elo (loss, but error is large)
    // 0.0.66 move-streams no-research  vs  0.0.65 move-streams     test 1      7.3 +/- 12.9 Elo (gain, but error is large)
    // 0.0.66 move-streams no-research  vs  0.0.65 move-streams     test 2      7.6 +/- 7.3 Elo (gain)

    uci();
}
