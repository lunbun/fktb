#include "test.h"

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <chrono>

#include "engine/piece.h"
#include "engine/move_list.h"
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



// Move generation test
void Tests::moveGenTest(const std::string &fen) {
    Board board = Board::fromFen(fen);

    RootMoveList moves = MoveGeneration::generateRoot(board);

    moves.score(board);
    moves.sort();

    while (!moves.empty()) {
        Move move = moves.dequeue();
        std::cout << move.debugName(board) << std::endl;
    }
}



// Fixed depth search test
void Tests::fixedDepthTest(const std::string &fen, uint16_t depth) {
    // Use mainly for benchmarking
    Board board = Board::fromFen(fen);

    TranspositionTable table(2097152);
    SearchDebugInfo debugInfo;
    FixedDepthSearcher searcher(board, depth, table, debugInfo);
    SearchLine bestLine = searcher.search();

    std::cout << "Best move: " << bestLine.moves[0].debugName(board) << std::endl;
    std::cout << "Score: " << bestLine.score << std::endl;
    std::cout << "Nodes: " << formatWithExact(debugInfo.nodeCount()) << std::endl;
    std::cout << "Transposition hits: " << formatWithExact(debugInfo.transpositionHits()) << std::endl;
    std::cout << "Time: " << debugInfo.elapsed().count() << "ms" << std::endl;
}



// Iterative deepening search test
void Tests::iterativeTest(const std::string &fen, uint16_t depth) {
    Board board = Board::fromFen(fen);

    IterativeSearcher searcher(std::thread::hardware_concurrency());

    volatile bool isComplete = false;

    searcher.addIterationCallback([&board, &searcher, &isComplete, depth](const SearchResult &result) {
        std::cout << result.bestLine[0].debugName(board);
        std::cout << " depth " << result.depth;
        std::cout << " score " << result.score;
        std::cout << " nodes " << formatNumber(result.nodeCount);
        std::cout << " time " << result.elapsed.count() << "ms";
        std::cout << std::endl;

        if (result.depth == depth) {
            isComplete = true;

            searcher.stop();
        }
    });

    searcher.start(board);

    // Sleep until the search is complete
    while (!isComplete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}



// Zobrist hash test
void verifyHash(Board &board) {
    uint64_t hash = board.hash();

    // Converting to fen and back will always produce the correct hash, so we can use this to verify
    uint64_t hash2 = Board::fromFen(board.toFen()).hash();

    if (hash != hash2) {
        throw std::runtime_error("Hashes do not match. Fen: " + board.toFen());
    }
}

template<Color Side>
uint32_t hashTestSearch(Board &board, uint16_t depth) {
    if (depth == 0) {
        return 1;
    }

    AlignedMoveEntry moveBuffer[MaxMoveCount];
    MoveEntry *movesStart = MoveEntry::fromAligned(moveBuffer);

    MoveEntry *movesEnd = MoveGeneration::generate<Side, false>(board, movesStart);

    MovePriorityQueue moves(movesStart, movesEnd);

    uint32_t nodeCount = 0;

    while (!moves.empty()) {
        Move move = moves.dequeue();

        MakeMoveInfo info = board.makeMove(move);

        verifyHash(board);

        nodeCount += hashTestSearch<~Side>(board, depth - 1);

        board.unmakeMove(move, info);

        verifyHash(board);
    }

    return nodeCount;
}

void Tests::hashTest(const std::string &fen, uint16_t depth) {
    Board board = Board::fromFen(fen);

    auto start = std::chrono::steady_clock::now();

    uint32_t nodeCount;
    if (board.turn() == Color::White) {
        nodeCount = hashTestSearch<Color::White>(board, depth);
    } else {
        nodeCount = hashTestSearch<Color::Black>(board, depth);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Hash test passed successfully." << std::endl;
    std::cout << "Nodes: " << formatWithExact(nodeCount) << std::endl;
    std::cout << "Time: " << duration << "ms" << std::endl;
}



// Transposition table lock test
void transpositionWriteNoLock(TranspositionTable &table, uint64_t key, int32_t value,
    std::chrono::steady_clock::time_point end) {
    while (std::chrono::steady_clock::now() < end) {
        table.debugStoreWithoutLock(key, value, TranspositionTable::Flag::Exact, Move::invalid(), value);
        table.debugStoreWithoutLock(key, value, TranspositionTable::Flag::Exact, Move::invalid(), value);
        table.debugStoreWithoutLock(key, value, TranspositionTable::Flag::Exact, Move::invalid(), value);
        table.debugStoreWithoutLock(key, value, TranspositionTable::Flag::Exact, Move::invalid(), value);
        table.debugStoreWithoutLock(key, value, TranspositionTable::Flag::Exact, Move::invalid(), value);
    }
}

void transpositionWriteLocked(TranspositionTable &table, uint64_t key, int32_t value,
    std::chrono::steady_clock::time_point end) {
    while (std::chrono::steady_clock::now() < end) {
        table.store(key, value, TranspositionTable::Flag::Exact, Move::invalid(), value);
        table.store(key, value, TranspositionTable::Flag::Exact, Move::invalid(), value);
        table.store(key, value, TranspositionTable::Flag::Exact, Move::invalid(), value);
        table.store(key, value, TranspositionTable::Flag::Exact, Move::invalid(), value);
        table.store(key, value, TranspositionTable::Flag::Exact, Move::invalid(), value);
    }
}

void transpositionCorruptionDetection(TranspositionTable &table, uint64_t key, uint32_t &corruptionCount,
    std::chrono::steady_clock::time_point end) {
    while (std::chrono::steady_clock::now() < end) {
        TranspositionTable::LockedEntry entry = table.load(key);

        if (entry->depth() != entry->bestScore()) {
            corruptionCount++;
        }
    }
}

void Tests::transpositionLockTest() {
    // The way this test works:
    //  1. First, we test a control case where we don't use any locks, to make sure that we can accurately detect if
    //      the transposition table is being corrupted.
    //  2. Then, test using locks, and check for corruption.
    //
    // (we are just using depth and bestScore as a way to detect corruption, they don't actually mean anything here)
    // Thread 1 will try to write:
    //      depth = 1, bestScore = 1
    // Thread 2 will try to write:
    //      depth = 2, bestScore = 2
    // Thread 3 will read, and if these two values are not equal, it means the table is corrupted.

    TranspositionTable table(1);

    // Control case, no locking
    {
        auto start = std::chrono::steady_clock::now();
        auto end = start + std::chrono::milliseconds(500);
        uint32_t corruptionCount = 0;

        std::thread thread1(transpositionWriteNoLock, std::ref(table), 0, 1, end);
        std::thread thread2(transpositionWriteNoLock, std::ref(table), 0, 2, end);
        std::thread thread3(transpositionCorruptionDetection, std::ref(table), 0, std::ref(corruptionCount), end);

        thread1.join();
        thread2.join();
        thread3.join();

        std::cout << "Control case (no locking): " << corruptionCount << " corruptions detected." << std::endl;
    }

    // Test case, with locking
    {
        auto start = std::chrono::steady_clock::now();
        auto end = start + std::chrono::milliseconds(500);
        uint32_t corruptionCount = 0;

        std::thread thread1(transpositionWriteLocked, std::ref(table), 0, 1, end);
        std::thread thread2(transpositionWriteLocked, std::ref(table), 0, 2, end);
        std::thread thread3(transpositionCorruptionDetection, std::ref(table), 0, std::ref(corruptionCount), end);

        thread1.join();
        thread2.join();
        thread3.join();

        std::cout << "Test case (with locking): " << corruptionCount << " corruptions detected." << std::endl;
    }
}



// Perft test
template<Color Side>
uint32_t perftSearch(Board &board, uint16_t depth) {
    if (depth == 0) {
        return 1;
    }

    AlignedMoveEntry moveBuffer[256];
    MoveEntry *movesStart = MoveEntry::fromAligned(moveBuffer);

    MoveEntry *movesEnd = MoveGeneration::generate<Side, false>(board, movesStart);

    MovePriorityQueue moves(movesStart, movesEnd);

    uint32_t nodeCount = 0;

    while (!moves.empty()) {
        Move move = moves.dequeue();

        MakeMoveInfo info = board.makeMove(move);

        nodeCount += perftSearch<~Side>(board, depth - 1);

        board.unmakeMove(move, info);
    }

    return nodeCount;
}

void Tests::perft(const std::string &fen, uint16_t depth) {
    Board board = Board::fromFen(fen);

    auto start = std::chrono::steady_clock::now();

    uint32_t nodeCount;
    if (board.turn() == Color::White) {
        nodeCount = perftSearch<Color::White>(board, depth);
    } else {
        nodeCount = perftSearch<Color::Black>(board, depth);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Perft test results:" << std::endl;
    std::cout << "Nodes: " << formatWithExact(nodeCount) << std::endl;
    std::cout << "Time: " << duration << "ms" << std::endl;
}
