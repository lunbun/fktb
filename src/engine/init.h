#pragma once

#include "engine/board/bitboard.h"
#include "engine/hash/transposition.h"

namespace EngineInit {
    void init() {
        Bitboards::maybeInit();
        Zobrist::maybeInit();
    }
}
