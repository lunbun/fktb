#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <chrono>

#include "uci.h"
#include "engine/movegen.h"
#include "engine/transposition.h"
#include "engine/fixed_search.h"
#include "engine/evaluation.h"

std::string formatNumber(int64_t number, int64_t divisor, char suffix) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << (number / (double) divisor) << suffix;
    return ss.str();
}

std::string formatNumber(int64_t number) {
    if (number < 1000) {
        return std::to_string(number);
    } else if (number < 1000000) {
        return formatNumber(number, 1000, 'k');
    } else if (number < 1000000000) {
        return formatNumber(number, 1000000, 'M');
    } else {
        return formatNumber(number, 1000000000, 'B');
    }
}

void init() {
    Bitboards::maybeInit();
    Zobrist::maybeInit();
    Evaluation::maybeInit();
}

void moveGenTest() {
    Board board = Board::fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    MovePriorityQueueStack moves(1, 64);
    moves.push();
    if (board.turn() == Color::White) {
        MoveGenerator<Color::White> generator(board, moves);
        generator.generate();
        moves.score<Color::White>(board);
    } else {
        MoveGenerator<Color::Black> generator(board, moves);
        generator.generate();
        moves.score<Color::Black>(board);
    }

    while (!moves.empty()) {
        Move move = moves.dequeue();
        std::cout << move.debugName(board) << std::endl;
    }

    moves.pop();
}

void fixedDepthTest() {
    // Use mainly for benchmarking
    Board board = Board::fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    auto start = std::chrono::high_resolution_clock::now();

    TranspositionTable table(2097152);
    FixedDepthSearcher searcher(board, 7, table);
    SearchResult result = searcher.search();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Best move: " << result.bestLine[0].debugName(board) << std::endl;
    std::cout << "Score: " << result.score << std::endl;
    std::cout << "Nodes: " << formatNumber(result.nodeCount) << " (exact " << result.nodeCount << ")" << std::endl;
    std::cout << "Transposition hits: " << formatNumber(result.transpositionHits) << " (exact " << result.transpositionHits << ")" << std::endl;
    std::cout << "Time: " << duration << "ms" << std::endl;
}

void fixedDepthBenchmark(uint8_t count) {
    uint64_t totalSearchTime = 0;

    for (uint8_t i = 0; i < count; ++i) {
        Board board = Board::fromFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        auto start = std::chrono::high_resolution_clock::now();

        TranspositionTable table(2097152);
        FixedDepthSearcher searcher(board, 8, table);
        SearchResult result = searcher.search();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        totalSearchTime += duration;

        std::cout << "Iteration " << (int) i << " took " << duration << "ms" << std::endl;
    }

    std::cout << "Total search time: " << totalSearchTime << "ms" << std::endl;
    std::cout << "Average search time: " << ((double) totalSearchTime / (double) count) << "ms" << std::endl;
}

[[noreturn]] void uci() {
    UciHandler uci("chess engine", "me");

    uci.run();
}

int main() {
    init();

//    moveGenTest();
//    fixedDepthTest();
//    fixedDepthBenchmark(10);

    uci();

//    return 0;
}
