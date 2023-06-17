#include <iostream>

#include "engine/init.h"

#include "test.h"
#include "uci.h"

[[noreturn]] void uci() {
    std::string name = "FKTB 0.0.57";
#ifndef NDEBUG
    name += " Debug";
#endif // #ifndef NDEBUG
    UciHandler uci(name, "frogscats");

    uci.run();
}

int main() {
    EngineInit::init();

    // WHY THE KILLER TABLE CRASH IS HAPPENING:
    // --------------------------------------------------------------
    //  1. Search is started, then immediately stopped.
    //  2. Search stopping occurs from a separate thread than the one that started the search.
    //  3. FixedDepthSearcher does not check if the search has halted until FixedDepthSearcher::searchRoot.
    //  4. Another search starts, and it starts while the old one is stopping (because the search stopping occurs from a separate
    //              thread).
    //  5. Starting the new search destructs the heuristics tables, which in turn destructs the killer table.
    //  6. The old search is still running (because it hasn't checked if it has halted yet), and it tries to resize the destructed
    //              killer table.
    //  7. The killer table has already been freed, so resizing it tries to free it twice.
    //  8. BOOM heap corruption
    //
    // How to fix?
    // --------------------------------------------------------------
    //  1. Before we start a new search, we have to wait for the old search to actually stop.
    //  2. Also, unrelated bug I discovered while debugging this one: the search stop threads can actually "leak" into the next
    //              search, causing the next search to stop erroneously.
    uci();
}
