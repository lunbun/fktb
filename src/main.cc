#include <iostream>
#include <thread>
#include <chrono>

#include "engine/board.h"
#include "engine/engine.h"

// Single threaded
//White Rook from f7 to f8 2774 4185972 1187462
//Black Queen from e2 to c2 1491 2260957 257312
//White Rook from f8 to a8 capturing Black King 630 1050911 222607
//Black Queen from c2 to h7 capturing White King 82 107874 43968
//White Rook from a8 to a7 158 180804 171940
//Black Queen from h7 to g6 120 118614 117923
//White Rook from a7 to b7 85 75943 101399
//Black Queen from g6 to e6 125 122080 121418

//void test(const Engine &engine, Board &board) {
//    auto start = std::chrono::high_resolution_clock::now();
//
//    SearchNode node = engine.search(board, 6);
//
//    auto end = std::chrono::high_resolution_clock::now();
//    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
//
//    std::cout << node.move->debugName() << " " << duration << " " << node.count << " " << node.transpositionHits << std::endl;
//
//    board.makeMove(node.move.value());
//}

int main() {
    Engine engine(12);

    Board board = Board::fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    engine.startIterativeSearch(board);

    std::this_thread::sleep_for(std::chrono::seconds(60));

    return 0;
}
