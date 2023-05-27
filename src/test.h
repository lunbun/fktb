#pragma once

#include <cstdint>
#include <string>

namespace Tests {
    // Prints out all the moves for a given position.
    void moveGenTest(const std::string &fen);

    // Runs a fixed depth search on a given position.
    void fixedDepthTest(const std::string &fen, uint16_t depth);

    // Runs an iterative deepening search on a given position.
    void iterativeTest(const std::string &fen, uint16_t depth);

    // Verifies that the Zobrist hash is working correctly.
    void hashTest(const std::string &fen, uint16_t depth);

    // Performs a perft test on a given position.
    void perft(const std::string &fen, uint16_t depth);
}
