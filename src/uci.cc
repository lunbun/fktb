#include "uci.h"

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <optional>
#include <thread>
#include <chrono>
#include <cassert>

#include "test.h"
#include "engine/mutex.h"
#include "engine/board/board.h"
#include "engine/search/score.h"
#include "engine/move/movegen.h"
#include "engine/search/iterative_search.h"

namespace FKTB::UCI {

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



// Search stopping needs to be asynchronous from a separate thread so that the main thread can continue to process input while we
// wait for a duration/node count to pass.
// TODO: Add a destructor to properly kill the thread.
class Handler::SearchStopThread {
public:
    explicit SearchStopThread(Handler &uci);

    void limitNodes(uint64_t nodeCount);
    void limitTime(std::chrono::milliseconds duration);

    void enable();
    void disable();

private:
    Handler &uci_;
    std::thread thread_;

    ami::mutex mutex_;
    bool isEnabled_ = false;
    std::optional<uint64_t> nodeLimit_ = std::nullopt;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>> timeLimit_ = std::nullopt;

    [[noreturn]] void loop();
};

Handler::SearchStopThread::SearchStopThread(Handler &uci) : uci_(uci) {
    this->thread_ = std::thread(&SearchStopThread::loop, this);
    this->thread_.detach();
}

void Handler::SearchStopThread::limitNodes(uint64_t nodeCount) {
    assert(!this->mutex_.locked_by_caller() && "SearchStopThread::limitNodes() must not be called with the mutex locked.");
    std::lock_guard lock(this->mutex_);

    // If there is already a node limit, choose whichever is smaller, since that one will always stop the search first.
    if (!this->nodeLimit_.has_value() || nodeCount < this->nodeLimit_.value()) {
        this->nodeLimit_ = nodeCount;
    }
}

void Handler::SearchStopThread::limitTime(std::chrono::milliseconds duration) {
    assert(!this->mutex_.locked_by_caller() && "SearchStopThread::limitTime() must not be called with the mutex locked.");
    std::lock_guard lock(this->mutex_);

    // If there is already a time limit, choose whichever is sooner, since that one will always stop the search first.
    auto timeLimit = std::chrono::steady_clock::now() + duration;
    if (!this->timeLimit_.has_value() || timeLimit < this->timeLimit_.value()) {
        this->timeLimit_ = timeLimit;
    }
}

void Handler::SearchStopThread::enable() {
    assert(!this->mutex_.locked_by_caller() && "SearchStopThread::enable() must not be called with the mutex locked.");
    std::lock_guard lock(this->mutex_);
    this->isEnabled_ = true;
}

void Handler::SearchStopThread::disable() {
    assert(!this->mutex_.locked_by_caller() && "SearchStopThread::disable() must not be called with the mutex locked.");
    std::lock_guard lock(this->mutex_);
    this->isEnabled_ = false;
    this->nodeLimit_ = std::nullopt;
    this->timeLimit_ = std::nullopt;
}

[[noreturn]] void Handler::SearchStopThread::loop() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        bool shouldStop = false;
        {
            std::lock_guard lock(this->mutex_);

            if (!this->isEnabled_) {
                continue;
            }

            // Node limit
            shouldStop |= (this->nodeLimit_.has_value() && this->uci_.searcher_->stats().nodeCount() >= this->nodeLimit_.value());

            // Time limit
            shouldStop |= (this->timeLimit_.has_value() && std::chrono::steady_clock::now() >= this->timeLimit_.value());
        }

        // Stop the search if necessary.
        if (shouldStop) {
            this->uci_.stopSearch();
        }
    }
}



Handler::Handler(std::string name, std::string author) : name_(std::move(name)), author_(std::move(author)),
                                                               board_(nullptr) {
//    this->searcher_ = std::make_unique<IterativeSearcher>(std::thread::hardware_concurrency());
    this->searcher_ = std::make_unique<IterativeSearcher>(1);
    this->searcher_->addIterationCallback([this](const SearchResult &result) {
        this->iterationCallback(result);
    });

    this->searchStopThread_ = std::make_unique<SearchStopThread>(*this);
}

Handler::~Handler() = default;

[[noreturn]] void Handler::run() {
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

void Handler::error(const std::string &message) {
    std::cerr << message << std::endl;
}

void Handler::handleInput(const std::string &input) {
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

void Handler::handleUci(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("uci command does not take arguments");
    }

    std::cout << "id name " << this->name_ << std::endl;
    std::cout << "id author " << this->author_ << std::endl;
    std::cout << "uciok" << std::endl;
}

void Handler::handleDebug(TokenStream &tokens) {
    return this->error("Debug mode does not do anything");
}

void Handler::handleIsReady(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("isready command does not take arguments");
    }

    std::cout << "readyok" << std::endl;
}

void Handler::handleSetOption(TokenStream &tokens) {
    if (tokens.isEnd()) {
        return this->error("setoption command requires arguments");
    }

    return this->error("No options supported");
}

void Handler::handleUciNewGame(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("ucinewgame command does not take arguments");
    }

    this->board_ = nullptr;
}

void Handler::handlePosition(TokenStream &tokens) {
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

void Handler::handleGo(TokenStream &tokens) {
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

void Handler::handleStop(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("stop command does not take arguments");
    }

    this->stopSearch();
}

void Handler::handleQuit(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("quit command does not take arguments");
    }

    std::exit(0);
}



void Handler::handleTest(TokenStream &tokens) {
    const std::string &command = tokens.next();

    if (command == "movegen") {
        this->handleTestMoveGen(tokens);
    } else if (command == "print_fen") {
        this->handleTestPrintFen(tokens);
    } else {
        return this->error("Unknown test command: " + command);
    }
}

void Handler::handleTestMoveGen(TokenStream &tokens) {
    if (this->board_ == nullptr) {
        return this->error("No board set");
    }

    Tests::legalMoveGenTest(this->board_->toFen());
}

void Handler::handleTestPrintFen(TokenStream &tokens) {
    if (this->board_ == nullptr) {
        return this->error("No board set");
    }

    std::cout << this->board_->toFen() << std::endl;
}



void Handler::startSearch(const SearchOptions &options) {
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
        this->searchStopThread_->limitTime(std::chrono::milliseconds(timeLimit));
    }

    // Node limit
    if (options.nodes.has_value()) {
        this->searchStopThread_->limitNodes(options.nodes.value());
    }

    // Move time limit
    if (options.moveTime.has_value()) {
        int32_t moveTime = options.moveTime.value();
        this->searchStopThread_->limitTime(std::chrono::milliseconds(moveTime));
    }

    // Enable search stop thread
    this->searchStopThread_->enable();
}

void Handler::stopSearch() {
    if (!this->isSearching_) {
        return this->error("Not searching");
    }

    // Disable any search stop thread limits
    this->searchStopThread_->disable();

    // Stop the search
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

void Handler::iterationCallback(const SearchResult &result) {
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
        std::cout << "mate " << Score::mateMoves(result.score);
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

} // namespace FKTB::UCI
