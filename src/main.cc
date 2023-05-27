#include "engine/init.h"

#include "test.h"
#include "uci.h"

[[noreturn]] void uci() {
    UciHandler uci("chess engine", "me");

    uci.run();
}

int main() {
    EngineInit::init();

    Tests::iterativeTest(Board::KiwiPeteFen, 7);

//    return 0;
}
