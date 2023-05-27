#include "test.h"

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <sstream>
#include <chrono>

#include "engine/piece.h"
#include "engine/move_queue.h"
#include "engine/movegen.h"
#include "engine/transposition.h"
#include "engine/fixed_search.h"
#include "engine/iterative_search.h"
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

std::string formatWithExact(int64_t number) {
    if (number < 1000) {
        return std::to_string(number);
    } else {
        return formatNumber(number) + " (exact " + std::to_string(number) + ")";
    }
}

void Tests::moveGenTest(const std::string &fen) {
    Board board = Board::fromFen(fen);

    MovePriorityQueueStack moves(1, 64);
    moves.push();
    if (board.turn() == Color::White) {
        MoveGenerator<Color::White, false> generator(board, moves);
        generator.generate();
        moves.score<Color::White>(board);
    } else {
        MoveGenerator<Color::Black, false> generator(board, moves);
        generator.generate();
        moves.score<Color::Black>(board);
    }

    while (!moves.empty()) {
        Move move = moves.dequeue();
        std::cout << move.debugName(board) << std::endl;
    }

    moves.pop();
}

void Tests::fixedDepthTest(const std::string &fen, uint16_t depth) {
    // Use mainly for benchmarking
    Board board = Board::fromFen(fen);

    auto start = std::chrono::steady_clock::now();

    TranspositionTable table(2097152);
    FixedDepthSearcher searcher(board, depth, table);
    SearchResult result = searcher.search();

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Best move: " << result.bestLine[0].debugName(board) << std::endl;
    std::cout << "Score: " << result.score << std::endl;
    std::cout << "Nodes: " << formatWithExact(result.nodeCount) << std::endl;
    std::cout << "Transposition hits: " << formatWithExact(result.transpositionHits) << std::endl;
    std::cout << "Time: " << duration << "ms" << std::endl;
}

void Tests::iterativeTest(const std::string &fen, uint16_t depth) {
    Board board = Board::fromFen(fen);

    IterativeSearchThread thread;

    volatile bool isComplete = false;

    auto start = std::chrono::steady_clock::now();

    thread.addIterationCallback([&board, &thread, &isComplete, depth, start](const SearchResult &result) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

        std::cout << result.bestLine[0].debugName(board);
        std::cout << " depth " << result.depth;
        std::cout << " score " << result.score;
        std::cout << " nodes " << formatNumber(result.nodeCount);
        std::cout << " time " << duration << "ms";
        std::cout << std::endl;

        if (result.depth == depth) {
            isComplete = true;

            thread.stop();
        }
    });

    thread.start(board);

    // Sleep until the search is complete
    while (!isComplete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void verifyHash(Board &board) {
    uint64_t hash = board.hash();

    // Converting to fen and back will always produce the correct hash, so we can use this to verify
    uint64_t hash2 = Board::fromFen(board.toFen()).hash();

    if (hash != hash2) {
        throw std::runtime_error("Hashes do not match. Fen: " + board.toFen());
    }
}

template<Color Side>
uint32_t hashTestSearch(Board &board, MovePriorityQueueStack &moves, uint16_t depth) {
    if (depth == 0) {
        return 1;
    }

    MovePriorityQueueStackGuard movesStackGuard(moves);

    MoveGenerator<Side, false> generator(board, moves);
    generator.generate();

    uint32_t nodeCount = 0;

    while (!moves.empty()) {
        Move move = moves.dequeue();

        MakeMoveInfo info = board.makeMove(move);

        verifyHash(board);

        nodeCount += hashTestSearch<~Side>(board, moves, depth - 1);

        board.unmakeMove(move, info);

        verifyHash(board);
    }

    return nodeCount;
}

void Tests::hashTest(const std::string &fen, uint16_t depth) {
    MovePriorityQueueStack moves(depth, 64 * depth);

    Board board = Board::fromFen(fen);

    auto start = std::chrono::steady_clock::now();

    uint32_t nodeCount;
    if (board.turn() == Color::White) {
        nodeCount = hashTestSearch<Color::White>(board, moves, depth);
    } else {
        nodeCount = hashTestSearch<Color::Black>(board, moves, depth);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Hash test passed successfully." << std::endl;
    std::cout << "Nodes: " << formatWithExact(nodeCount) << std::endl;
    std::cout << "Time: " << duration << "ms" << std::endl;
}

template<Color Side>
uint32_t perftSearch(Board &board, MovePriorityQueueStack &moves, uint16_t depth) {
    if (depth == 0) {
        return 1;
    }

    MovePriorityQueueStackGuard movesStackGuard(moves);

    MoveGenerator<Side, false> generator(board, moves);
    generator.generate();

    uint32_t nodeCount = 0;

    while (!moves.empty()) {
        Move move = moves.dequeue();

        MakeMoveInfo info = board.makeMove(move);

        nodeCount += perftSearch<~Side>(board, moves, depth - 1);

        board.unmakeMove(move, info);
    }

    return nodeCount;
}

void Tests::perft(const std::string &fen, uint16_t depth) {
    MovePriorityQueueStack moves(depth, 64 * depth);

    Board board = Board::fromFen(fen);

    auto start = std::chrono::steady_clock::now();

    uint32_t nodeCount;
    if (board.turn() == Color::White) {
        nodeCount = perftSearch<Color::White>(board, moves, depth);
    } else {
        nodeCount = perftSearch<Color::Black>(board, moves, depth);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Perft test results:" << std::endl;
    std::cout << "Nodes: " << formatWithExact(nodeCount) << std::endl;
    std::cout << "Time: " << duration << "ms" << std::endl;
}
