#include <iostream>

#include "engine/init.h"

#include "test.h"
#include "uci.h"

[[noreturn]] void uci() {
    UciHandler uci("FKTB 0.0.51", "frogscats");

    uci.run();
}

int main() {
    EngineInit::init();

    uci();
}
