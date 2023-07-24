#pragma once

#include "engine/board/bitboard.h"
#include "engine/hash/transposition.h"
#include "engine/search/fixed_search.h"

namespace FKTB {

void init() {
    Bitboards::init();
    Zobrist::init();
}

} // namespace FKTB
