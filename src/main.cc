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

//    Tests::unmakeMoveTest(Board::KiwiPeteFen);
//    Tests::moveGenTest(Board::KiwiPeteFen);
//    Tests::fixedDepthTest(Board::KiwiPeteFen, 7);

    uci();

//    return 0;
}
