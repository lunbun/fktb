#include "uci.h"

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <optional>
#include <thread>

#include "test.h"
#include "engine/board/board.h"
#include "engine/search/score.h"
#include "engine/move/movegen.h"
#include "engine/search/iterative_search.h"

TokenStream::TokenStream(const std::string &input) : index_(0), tokens_() {
    std::stringstream ss(input);
    std::string item;

    while (getline(ss, item, ' ')) {
        this->tokens_.push_back(item);
    }
}

const std::string &TokenStream::next() {
    return this->tokens_[this->index_++];
}



UciHandler::UciHandler(std::string name, std::string author) : name_(std::move(name)), author_(std::move(author)),
                                                               board_(nullptr) {
//    this->searcher_ = std::make_unique<IterativeSearcher>(std::thread::hardware_concurrency());
    this->searcher_ = std::make_unique<IterativeSearcher>(1);
    this->searcher_->addIterationCallback([this](const SearchResult &result) {
        this->iterationCallback(result);
    });
}

void UciHandler::run() {
    while (true) {
        std::string input;

        std::getline(std::cin, input);

        try {
            this->handleInput(input);
        } catch (const std::exception &e) {
            this->error(std::string("Uncaught exception: ") + e.what());
        }
    }
}

void UciHandler::error(const std::string &message) {
    std::cerr  << message << std::endl;
}

void UciHandler::handleInput(const std::string &input) {
    TokenStream tokens(input);
    if (tokens.isEnd()) {
        return this->error("Empty input");
    }

    const std::string &command = tokens.next();
    if (command == "uci") {
        this->handleUci(tokens);
    } else if (command == "debug") {
        this->handleDebug(tokens);
    } else if (command == "isready") {
        this->handleIsReady(tokens);
    } else if (command == "setoption") {
        this->handleSetOption(tokens);
    } else if (command == "ucinewgame") {
        this->handleUciNewGame(tokens);
    } else if (command == "position") {
        this->handlePosition(tokens);
    } else if (command == "go") {
        this->handleGo(tokens);
    } else if (command == "stop") {
        this->handleStop(tokens);
    } else if (command == "quit") {
        this->handleQuit(tokens);
    } else if (command == "test") {
        this->handleTest(tokens);
    } else {
        return this->error("Unknown command: " + command);
    }
}

void UciHandler::handleUci(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("uci command does not take arguments");
    }

    std::cout << "id name " << this->name_ << std::endl;
    std::cout << "id author " << this->author_ << std::endl;
    std::cout << "uciok" << std::endl;
}

void UciHandler::handleDebug(TokenStream &tokens) {
    return this->error("Debug mode does not do anything");
}

void UciHandler::handleIsReady(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("isready command does not take arguments");
    }

    std::cout << "readyok" << std::endl;
}

void UciHandler::handleSetOption(TokenStream &tokens) {
    if (tokens.isEnd()) {
        return this->error("setoption command requires arguments");
    }

    return this->error("No options supported");
}

void UciHandler::handleUciNewGame(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("ucinewgame command does not take arguments");
    }

    this->board_ = nullptr;
}

void UciHandler::handlePosition(TokenStream &tokens) {
    if (tokens.isEnd()) {
        return this->error("position command requires arguments");
    }

    const std::string &positionType = tokens.next();
    if (positionType == "startpos") {
        this->board_ = std::make_unique<Board>(Board::startingPosition());
    } else if (positionType == "fen") {
        std::string fen;

        while (!tokens.isEnd() && tokens.peek() != "moves") {
            fen += tokens.next() + " ";
        }

        this->board_ = std::make_unique<Board>(Board::fromFen(fen));
    } else {
        return this->error("position command requires 'startpos' or 'fen' as first argument");
    }

    if (!tokens.isEnd()) {
        const std::string &moves = tokens.next();
        if (moves != "moves") {
            return this->error("position command requires 'moves' as second argument");
        }

        while (!tokens.isEnd()) {
            const std::string &moveString = tokens.next();

            Move move = Move::fromUci(moveString, *this->board_);
            this->board_->makeMove<MakeMoveType::All>(move);
        }
    }
}

void UciHandler::handleGo(TokenStream &tokens) {
    if (tokens.isEnd()) {
        return this->error("go command requires arguments");
    }

    SearchOptions options;

    while (!tokens.isEnd()) {
        const std::string &command = tokens.next();

        if (command == "infinite") {
            options.infinite = true;
        } else if (command == "wtime") {
            if (tokens.isEnd()) return this->error("wtime command requires an argument");
            options.timeControl.time.white() = std::stoi(tokens.next());
        } else if (command == "btime") {
            if (tokens.isEnd()) return this->error("btime command requires an argument");
            options.timeControl.time.black() = std::stoi(tokens.next());
        } else if (command == "winc") {
            if (tokens.isEnd()) return this->error("winc command requires an argument");
            options.timeControl.increment.white() = std::stoi(tokens.next());
        } else if (command == "binc") {
            if (tokens.isEnd()) return this->error("binc command requires an argument");
            options.timeControl.increment.black() = std::stoi(tokens.next());
        } else if (command == "depth") {
            if (tokens.isEnd()) return this->error("depth command requires an argument");
            options.depth = std::stoi(tokens.next());
        } else if (command == "nodes") {
            if (tokens.isEnd()) return this->error("nodes command requires an argument");
            options.nodes = std::stoull(tokens.next());
        } else if (command == "movetime") {
            if (tokens.isEnd()) return this->error("movetime command requires an argument");
            options.moveTime = std::stoi(tokens.next());
        } else {
            this->error("Unknown/unsupported command: " + command);
        }
    }

    this->startSearch(options);
}

void UciHandler::handleStop(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("stop command does not take arguments");
    }

    this->stopSearch();
}

void UciHandler::handleQuit(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("quit command does not take arguments");
    }

    std::exit(0);
}



void UciHandler::handleTest(TokenStream &tokens) {
    const std::string &command = tokens.next();

    if (command == "movegen") {
        this->handleTestMoveGen(tokens);
    } else if (command == "print_fen") {
        this->handleTestPrintFen(tokens);
    } else {
        return this->error("Unknown test command: " + command);
    }
}

void UciHandler::handleTestMoveGen(TokenStream &tokens) {
    if (this->board_ == nullptr) {
        return this->error("No board set");
    }

    Tests::legalMoveGenTest(this->board_->toFen());
}

void UciHandler::handleTestPrintFen(TokenStream &tokens) {
    if (this->board_ == nullptr) {
        return this->error("No board set");
    }

    std::cout << this->board_->toFen() << std::endl;
}



void UciHandler::startSearch(const SearchOptions &options) {
    if (this->isSearching_) {
        return this->error("Already searching");
    }

    if (this->board_ == nullptr) {
        return this->error("No board set");
    }

    this->isSearching_ = true;
    this->searchOptions_ = options;

    this->searcher_->start(*this->board_);

    Color us = this->board_->turn();

    // Time control
    if (options.timeControl.time[us].has_value()) {
        int32_t time = options.timeControl.time[us].value();

        // Assume we have 40 moves left
        int32_t movesLeft = 40;

        int32_t timeLimit = time / movesLeft;
        this->stopSearchAfter(std::chrono::milliseconds(timeLimit));
    }

    // Node limit
    if (options.nodes.has_value()) {
        uint64_t nodes = options.nodes.value();
        this->stopSearchAfter([this, nodes]() {
            return this->searcher_->stats().nodeCount() >= nodes;
        });
    }

    // Move time limit
    if (options.moveTime.has_value()) {
        int32_t moveTime = options.moveTime.value();
        this->stopSearchAfter(std::chrono::milliseconds(moveTime));
    }
}

void UciHandler::stopSearchAfter(std::chrono::milliseconds duration) {
    // Start a new thread to stop the search after the given time
    // It's possible that we may run into thread safety issues here
    std::thread([this, duration]() {
        std::this_thread::sleep_for(duration);
        this->stopSearch();
    }).detach();
}

void UciHandler::stopSearchAfter(const std::function<bool()> &condition) {
    // Start a new thread to stop the search after the given number of nodes
    // It's possible that we may run into thread safety issues here
    std::thread([this, condition]() {
        while (this->isSearching_ && !condition()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        this->stopSearch();
    }).detach();
}

void UciHandler::stopSearch() {
    if (!this->isSearching_) {
        return this->error("Not searching");
    }

    SearchResult result = this->searcher_->stop();

    if (!result.isValid()) {
        // Search was stopped before it could find any move (this happens if the user stops the search immediately after it is
        // started)

        // We still need to print a bestmove, so just pick the first legal move we generate
        RootMoveList moves = MoveGeneration::generateLegalRoot(*this->board_);
        if (moves.empty()) {
            return this->error("Tried to search while in checkmate/stalemate");
        }

        result.bestLine = { moves.dequeue() };
    }

    std::cout << "bestmove " << result.bestLine[0].uci() << std::endl;

    // Search state needs to be reset last, in case any iteration callbacks are called between the time stopSearch() is called
    // and now.
    this->isSearching_ = false;
    this->searchOptions_ = std::nullopt;
}

void UciHandler::iterationCallback(const SearchResult &result) {
    if (!this->isSearching_ || !this->searchOptions_.has_value()) {
        return this->error("Iteration callback somehow called when not searching");
    }

    uint64_t millis = result.elapsed.count();

    // Calculate nodes per second
    uint64_t nps;
    if (millis == 0) {
        nps = result.nodeCount * 1000ULL;
    } else {
        nps = result.nodeCount * 1000ULL / millis;
    }

    // Print the search result
    std::cout << "info depth " << result.depth;
    std::cout << " score ";
    if (Score::isMate(result.score)) {
        std::cout << "mate " << Score::matePlies(result.score);
    } else {
        std::cout << "cp " << result.score;
    }
    std::cout << " time " << millis;
    std::cout << " nodes " << result.nodeCount;
    std::cout << " nps " << nps;
    std::cout << " pv ";
    for (uint32_t i = 0; i < result.bestLine.size(); i++) {
        std::cout << result.bestLine[i].uci();

        if (i != result.bestLine.size() - 1) {
            std::cout << " ";
        }
    }
    std::cout << std::endl;

    // Depth limit
    if (this->searchOptions_->depth.has_value() && result.depth >= this->searchOptions_->depth.value()) {
        // Spawn a new thread to stop the search (cannot stop it from this thread because this thread IS the search
        // thread, since we are in the search callback). Trying to stop it from this thread would cause a deadlock.
        std::thread([this]() {
            this->stopSearch();
        }).detach();
    }
}
