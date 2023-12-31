#include "test.h"

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <chrono>
#include <cassert>

#include "engine/board/piece.h"
#include "engine/move/movegen.h"
#include "engine/move/move_list.h"
#include "engine/eval/evaluation.h"
#include "engine/hash/transposition.h"
#include "engine/search/fixed_search.h"
#include "engine/search/iterative_search.h"

namespace FKTB {

namespace {

std::string formatNumber(uint64_t number, uint64_t divisor, char suffix) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << (static_cast<double>(number) / static_cast<double>(divisor)) << suffix;
    return ss.str();
}

std::string formatNumber(uint64_t number) {
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

std::string formatWithExact(uint64_t number) {
    if (number < 1000) {
        return std::to_string(number);
    } else {
        return formatNumber(number) + " (exact " + std::to_string(number) + ")";
    }
}

} // namespace



// Move generation test
void Tests::pseudoLegalMoveGenTest(const std::string &fen) {
    Board board = Board::fromFen(fen);

    AlignedMoveEntry moveBuffer[MaxMoveCount];
    MoveEntry *movesStart = MoveEntry::fromAligned(moveBuffer);

    MoveEntry *movesEnd = (board.turn() == Color::White) ?
        MoveGeneration::generate<Color::White, MoveGeneration::Type::PseudoLegal>(board, movesStart) :
        MoveGeneration::generate<Color::Black, MoveGeneration::Type::PseudoLegal>(board, movesStart);

    MovePriorityQueue moves(movesStart, movesEnd);
    if (board.turn() == Color::White) {
        MoveOrdering::score<Color::White, MoveOrdering::Type::AllNoHistory>(moves, board, nullptr);
    } else {
        MoveOrdering::score<Color::Black, MoveOrdering::Type::AllNoHistory>(moves, board, nullptr);
    }

    while (!moves.empty()) {
        Move move = moves.dequeue();
        std::cout << move.debugName(board) << std::endl;
    }
}

void Tests::legalMoveGenTest(const std::string &fen) {
    Board board = Board::fromFen(fen);

    RootMoveList moves = MoveGeneration::generateLegalRoot(board);

    MoveOrdering::score<MoveOrdering::Type::AllNoHistory>(moves, board, nullptr);
    moves.sort();

    while (!moves.empty()) {
        Move move = moves.dequeue();
        std::cout << move.debugName(board) << std::endl;
    }
}



// Move ordering test
void Tests::moveOrderingTest(const std::string &fen, const std::vector<std::string> &movesSequence) {
    Board board = Board::fromFen(fen);

    // Run a search to get history heuristic data
    TranspositionTable table(2097152);
    HeuristicTables heuristics;
    SearchStatistics stats;
    FixedDepthSearcher searcher(board, 9, table, heuristics, stats);
    static_cast<void>(searcher.search());

    // Make the moves
    for (const std::string &moveStr : movesSequence) {
        Move move = Move::fromUci(moveStr, board);
        board.makeMove<MakeMoveType::All>(move);
    }

    RootMoveList movesList = MoveGeneration::generateLegalRoot(board);

    MoveOrdering::score<MoveOrdering::Type::All>(movesList, board, &heuristics.history);
    movesList.sort();

    const std::vector<MoveEntry> &moves = movesList.moves();

    // Iterate in reverse order, since the moves are sorted from smallest to largest (this was done to optimize dequeueing)
    for (auto it = moves.rbegin(); it != moves.rend(); it++) {
        std::cout << it->move.debugName(board) << " " << it->score << std::endl;
    }
}



// Repetition test
void Tests::repetitionTest(const std::string &fen, const std::vector<std::string> &movesSequence) {
    Board board = Board::fromFen(fen);

    // Make the moves
    for (const std::string &moveStr : movesSequence) {
        Move move = Move::fromUci(moveStr, board);

        std::cout << move.debugName(board);

        board.makeMove<MakeMoveType::All>(move);

        if (board.isTwofoldRepetition()) {
            std::cout << " is a repetition" << std::endl;
        } else {
            std::cout << " is not a repetition" << std::endl;
        }
    }
}



// Benchmark
void Tests::benchmark() {
    // Run the fixed depth search on starting position, depth 8 five times
    std::chrono::milliseconds total;

    for (int i = 0; i < 5; i++) {
        total += fixedDepthTest(Board::StartingFen, 8);
    }

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Total time: " << total.count() << "ms" << std::endl;
    std::cout << "Average time: " << total.count() / 5 << "ms" << std::endl;
}



// Fixed depth search test
std::chrono::milliseconds Tests::fixedDepthTest(const std::string &fen, uint16_t depth) {
    // Use mainly for benchmarking
    Board board = Board::fromFen(fen);

    TranspositionTable table(2097152);
    HeuristicTables heuristics;
    SearchStatistics stats;
    FixedDepthSearcher searcher(board, depth, table, heuristics, stats);
    SearchLine bestLine = searcher.search();

    std::cout << "Best move: " << bestLine.moves[0].debugName(board) << std::endl;
    std::cout << "Score: " << bestLine.score << std::endl;
    std::cout << "Nodes: " << formatWithExact(stats.nodeCount()) << std::endl;
    std::cout << "Transposition hits: " << formatWithExact(stats.transpositionHits()) << std::endl;
    std::cout << "Time: " << stats.elapsed().count() << "ms" << std::endl;

    return stats.elapsed();
}



// Iterative deepening search test
void Tests::iterativeTest(const std::string &fen, uint16_t depth, uint32_t threads) {
    assert(threads == 1);

    Board board = Board::fromFen(fen);

    IterativeSearcher searcher(threads);

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



// Unmake move test
void Tests::unmakeMoveTest(const std::string &fen) {
    Board board = Board::fromFen(fen);

    RootMoveList moves = MoveGeneration::generateLegalRoot(board);

    while (!moves.empty()) {
        Move move = moves.dequeue();

        std::string beforeFen = board.toFen();
        Board beforeBoard = board.copy();

        MakeMoveInfo info = board.makeMove<MakeMoveType::All>(move);

        board.unmakeMove<MakeMoveType::All>(move, info);

        if (beforeFen != board.toFen()) {
            throw std::runtime_error("Fen does not match after unmake move. Fen: " + beforeFen);
        }

        // Check all the bitboards
        for (uint8_t i = 0; i < 2; ++i) {
            for (uint8_t j = 0; j < 6; ++j) {
                auto color = static_cast<Color>(i);
                auto type = static_cast<PieceType>(j);

                if (board.bitboard({ color, type }) != beforeBoard.bitboard({ color, type })) {
                    throw std::runtime_error("Bitboard does not match after unmake move. Fen: " + beforeFen);
                }
            }
        }
    }

    std::cout << "Unmake move test passed" << std::endl;
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

    MoveEntry *movesEnd = MoveGeneration::generate<Side, MoveGeneration::Type::Legal>(board, movesStart);

    MovePriorityQueue moves(movesStart, movesEnd);

    uint32_t nodeCount = 0;

    while (!moves.empty()) {
        Move move = moves.dequeue();

        MakeMoveInfo info = board.makeMove<MakeMoveType::All>(move);

        verifyHash(board);

        nodeCount += hashTestSearch<~Side>(board, depth - 1);

        board.unmakeMove<MakeMoveType::All>(move, info);

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



// Perft test
template<Color Side>
uint64_t perftSearch(Board &board, uint16_t depth) {
    AlignedMoveEntry moveBuffer[256];
    MoveEntry *movesStart = MoveEntry::fromAligned(moveBuffer);

    MoveEntry *movesEnd = MoveGeneration::generate<Side, MoveGeneration::Type::Legal>(board, movesStart);
    if (depth == 1) {
        // If we are at the bottom of the search, just return the number of moves
        return movesEnd - movesStart;
    }

    uint64_t nodeCount = 0;

    while (movesEnd != movesStart) {
        Move move = (--movesEnd)->move;

        MakeMoveInfo info = board.makeMove<MakeMoveType::AllNoTurn>(move);

        nodeCount += perftSearch<~Side>(board, depth - 1);

        board.unmakeMove<MakeMoveType::AllNoTurn>(move, info);
    }

    return nodeCount;
}

void Tests::perft(const std::string &fen, uint16_t depth) {
    Board board = Board::fromFen(fen);

    auto start = std::chrono::steady_clock::now();

    uint64_t nodeCount;
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

} // namespace FKTB
