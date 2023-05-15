#include <iostream>
#include <chrono>

#include "engine/board.h"
#include "engine/search.h"
#include "engine/iterative_search.h"

// White Pawn from a2 to a3 -100 6773648 155925 7789

int main() {
    Board board = Board::fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

//    IterativeSearcher searcher(board);
//
//    searcher.startSearch();

    FixedDepthSearcher searcher(board, 6);

    auto start = std::chrono::high_resolution_clock::now();

    SearchNode node = searcher.searchRoot();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << node.move->debugName() << " " << node.score << " "
              << node.nodeCount << " " << node.transpositionHits << " " << duration << std::endl;

    return 0;
}
