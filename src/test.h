#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace Tests {
    // Prints out all the moves for a given position.
    void pseudoLegalMoveGenTest(const std::string &fen);
    void legalMoveGenTest(const std::string &fen);

    // Prints out all moves and their ordering scores for a given position.
    //
    // In order for the move ordering to have history heuristic data to work with, a search must be run on a position that has the
    // desired position to analyze as a child node. This is why we need the movesSequence parameter.
    void moveOrderingTest(const std::string &fen, const std::vector<std::string> &movesSequence);

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

    // Performs a perft test on a given position.
    void perft(const std::string &fen, uint16_t depth);
}
