#include <iostream>
#include <chrono>

#include "engine/board.h"
#include "engine/fixed_search.h"

// White Pawn from a2 to a3 5507187 127784 2505

int main() {
    Board board = Board::startingPosition();

    FixedDepthSearcher searcher(board, 6);

    auto start = std::chrono::high_resolution_clock::now();

    SearchResult result = searcher.searchRoot();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << result.bestLine[0].debugName() << " " << result.nodeCount << " " << result.transpositionHits << " " << duration << std::endl;
}
