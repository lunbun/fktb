#include <iostream>

#include "engine/init.h"

#include "test.h"
#include "uci.h"

[[noreturn]] void uci() {
    UciHandler uci("chess engine", "me");

    uci.run();
}

int main() {
    EngineInit::init();

    // TODO:
    //  Make sure that castling rights are actually working with Zobrist hashing

    uci();

//    return 0;
}
