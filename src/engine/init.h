#pragma once

#include "engine/board/bitboard.h"
#include "engine/hash/transposition.h"
#include "engine/eval/nnue.h"

namespace FKTB {

void init() {
    Bitboards::init();
    Zobrist::init();
    NNUE::init();
}

} // namespace FKTB
