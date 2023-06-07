#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace Tests {
    // Prints out all the moves for a given position.
    void pseudoLegalMoveGenTest(const std::string &fen);
    void legalMoveGenTest(const std::string &fen);

    // Prints out all moves and their ordering scores for a given position.
    void moveOrderingTest(const std::string &fen);

    // Runs a benchmark on the fixed depth search.
    void benchmark();

    // Runs a fixed depth search on a given position.
    std::chrono::milliseconds fixedDepthTest(const std::string &fen, uint16_t depth);

    // Runs an iterative deepening search on a given position.
    void iterativeTest(const std::string &fen, uint16_t depth, uint32_t threads);

    // Verifies that unmakeMove is working correctly.
    void unmakeMoveTest(const std::string &fen);

    // Verifies that the Zobrist hash is working correctly.
    void hashTest(const std::string &fen, uint16_t depth);

    // Verifies that the transposition table is locking correctly, and that no two threads can write/read at the same
    // time.
    void transpositionLockTest();

    // Performs a perft test on a given position.
    void perft(const std::string &fen, uint16_t depth);
}
