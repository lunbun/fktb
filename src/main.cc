#include <iostream>
#include <chrono>

#include "engine/board.h"
#include "engine/search.h"
#include "engine/iterative_search.h"

//depth 1 White Pawn from a2 to a3 20 0 0
//depth 2 White Pawn from a2 to a3 400 0 0
//depth 3 White Pawn from a2 to a3 8157 0 5
//depth 4 White Pawn from a2 to a3 104886 2976 68
//depth 5 White Pawn from b2 to b3 541512 9261 385
//depth 6 White Pawn from a2 to a3 8391095 43026 6188

int main() {
    Board board = Board::fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    IterativeSearcher searcher(board);

    searcher.startSearch();

//    FixedDepthSearcher searcher(board, 5);
//
//    auto start = std::chrono::high_resolution_clock::now();
//
//    FixedDepthSearchResult result = searcher.searchRoot();
//
//    auto end = std::chrono::high_resolution_clock::now();
//    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
//    std::cout << result.move->debugName() << " " << result.score << " "
//              << result.nodeCount << " " << result.transpositionHits << " " << duration << std::endl;

    return 0;
}
