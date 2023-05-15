#include <iostream>
#include <chrono>

#include "engine/board.h"
#include "engine/search.h"
#include "engine/iterative_search.h"

int main() {
    Board board = Board::fromFen("2k5/8/8/8/8/8/5p2/5QK1 b - - 0 1");

    IterativeSearcher searcher(board);

    searcher.startSearch();

//    FixedDepthSearcher searcher(board, 6);
//
//    auto start = std::chrono::high_resolution_clock::now();
//
//    SearchNode node = searcher.searchRoot();
//
//    auto end = std::chrono::high_resolution_clock::now();
//    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
//    std::cout << node.move->debugName() << " " << node.score << " "
//              << node.nodeCount << " " << node.transpositionHits << " " << duration << std::endl;

    return 0;
}
