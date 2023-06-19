#include <iostream>

#include "engine/init.h"

#include "test.h"
#include "uci.h"

[[noreturn]] void uci() {
    std::string name = "FKTB 0.0.58";
#ifndef NDEBUG
    name += " Debug";
#endif // #ifndef NDEBUG
    UciHandler uci(name, "frogscats");

    uci.run();
}

int main() {
    EngineInit::init();

    //  2. Also, unrelated bug I discovered while debugging this one: the search stop threads can actually "leak" into the next
    //              search, causing the next search to stop erroneously.
    uci();
}
