#include "uci.h"

#include <vector>
#include <string>
#include <sstream>
#include <iostream>

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
    this->searchThread_ = std::make_unique<IterativeSearchThread>();
    this->searchThread_->addIterationCallback([this](const SearchResult &result) {
        this->sendIterationResult(result);
    });
}

void UciHandler::loop() {
    while (true) {
        std::string input;

        std::getline(std::cin, input);

        this->handleInput(input);
    }
}

void UciHandler::error(const std::string &message) {
    if (!this->isDebug_) {
        return;
    }

    std::cout << message << std::endl;
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
    if (tokens.isEnd()) {
        return this->error("debug command requires arguments");
    }

    const std::string &arg = tokens.next();
    if (arg == "on") {
        this->isDebug_ = true;
    } else if (arg == "off") {
        this->isDebug_ = false;
    } else {
        return this->error("debug command requires 'on' or 'off' as first argument");
    }
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
            const std::string &move = tokens.next();
            this->board_->makeMove(Move::fromUci(move, *this->board_));
        }
    }
}

void UciHandler::handleGo(TokenStream &tokens) {
    if (tokens.isEnd()) {
        return this->error("go command requires arguments");
    }

    if (tokens.peek() != "infinite") {
        return this->error("Only 'go infinite' is supported");
    }

    this->nodeCount_ = 0;
    this->startTime_ = std::chrono::steady_clock::now();

    this->searchThread_->start(*this->board_);
}

void UciHandler::handleStop(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("stop command does not take arguments");
    }

    SearchResult result = this->searchThread_->stop();

    if (!result.isValid || result.bestLine.empty()) {
        return this->error("stop command called before search finished");
    }

    std::cout << "bestmove " << result.bestLine[0].uci() << std::endl;
}

void UciHandler::handleQuit(TokenStream &tokens) {
    if (!tokens.isEnd()) {
        return this->error("quit command does not take arguments");
    }

    std::exit(0);
}

void UciHandler::sendIterationResult(const SearchResult &result) {
    this->nodeCount_ += result.nodeCount;

    auto duration = std::chrono::steady_clock::now() - this->startTime_;
    uint32_t millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    uint64_t nps = this->nodeCount_ * 1000 / millis;

    std::cout << "info depth " << result.depth;
    std::cout << " score cp " << result.score;
    std::cout << " time " << millis;
    std::cout << " nodes " << this->nodeCount_;
    std::cout << " nps " << nps;
    std::cout << " pv ";
    for (Move move : result.bestLine) {
        std::cout << move.uci() << " ";
    }
    std::cout << std::endl;
}
