#pragma once

#include "bitboard.h"
#include "transposition.h"

namespace EngineInit {
    void init() {
        Bitboards::maybeInit();
        Zobrist::maybeInit();
    }
}
