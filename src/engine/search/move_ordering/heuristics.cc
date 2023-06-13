#include "heuristics.h"

#include <cstdlib>
#include <cstring>

// Initialize the totals to start at 1 to avoid division by zero. This won't really affect the results.
HistoryTable::HistoryTable() : total_(1, 1), table_() { }



KillerTable::KillerTable() {
    this->size_ = 0;
    this->table_ = nullptr;
}

KillerTable::~KillerTable() {
    std::free(this->table_);
}

void KillerTable::resize(uint16_t size) {
    assert(size > this->size_);

    auto newTable = static_cast<KillerTable::Ply *>(std::calloc(size, sizeof(KillerTable::Ply)));

    // Keep the old moves anchored to the end of the table and add new space to the front. This keeps the depth indices consistent
    // between resizes.
    std::memcpy(newTable + size - this->size_, this->table_, this->size_ * sizeof(KillerTable::Ply));

    std::free(this->table_);
    this->size_ = size;
    this->table_ = newTable;
}

void KillerTable::add(uint16_t depth, Move move) {
    assert(depth < this->size_);

    auto &killers = this->table_[depth];

    // Check if the move is already in the table.
    for (auto &killer : killers) {
        if (killer == move) {
            return;
        }
    }

    // Shift the moves down.
    for (uint8_t i = 0; i < KillerTable::MaxKillerMoves - 1; i++) {
        killers[i + 1] = killers[i];
    }

    // Insert the new move.
    killers[0] = move;
}



HeuristicTables::HeuristicTables() : history(), killers() { }
