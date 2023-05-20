#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <memory>

#include "engine/board.h"
#include "engine/iterative_search.h"

class TokenStream {
public:
    explicit TokenStream(const std::string &input);

    [[nodiscard]] inline bool isEnd() const { return this->index_ >= this->tokens_.size(); }
    [[nodiscard]] const std::string &next();
    [[nodiscard]] inline const std::string &peek() const { return this->tokens_[this->index_]; }

private:
    uint32_t index_;
    std::vector<std::string> tokens_;
};

class UciHandler {
public:
    UciHandler(std::string name, std::string author);

    [[noreturn]] void loop();

private:
    std::string name_, author_;
    bool isDebug_ = true;

    uint64_t nodeCount_ = 0;
    std::chrono::time_point<std::chrono::steady_clock> startTime_;

    std::unique_ptr<Board> board_;
    std::unique_ptr<IterativeSearchThread> searchThread_;

    void error(const std::string &message);

    void handleInput(const std::string &input);

    void handleUci(TokenStream &tokens);
    void handleDebug(TokenStream &tokens);
    void handleIsReady(TokenStream &tokens);
    void handleSetOption(TokenStream &tokens);
    void handleUciNewGame(TokenStream &tokens);
    void handlePosition(TokenStream &tokens);
    void handleGo(TokenStream &tokens);
    void handleStop(TokenStream &tokens);
    void handleQuit(TokenStream &tokens);



    void sendIterationResult(const SearchResult &result);
};
