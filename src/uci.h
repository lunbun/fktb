#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <optional>
#include <memory>

#include "engine/board.h"
#include "engine/iterative_search.h"

class TokenStream {
public:
    explicit TokenStream(const std::string &input);

    [[nodiscard]] bool isEnd() const { return this->index_ >= this->tokens_.size(); }
    [[nodiscard]] const std::string &next();
    [[nodiscard]] const std::string &peek() const { return this->tokens_[this->index_]; }

private:
    uint32_t index_;
    std::vector<std::string> tokens_;
};

struct SearchOptions {
    bool infinite = false;
    std::optional<int32_t> depth;
    std::optional<int32_t> moveTime;
};

class UciHandler {
public:
    UciHandler(std::string name, std::string author);

    [[noreturn]] void run();

private:
    std::string name_, author_;
    bool isDebug_ = true;

    bool isSearching_ = false;
    std::optional<SearchOptions> searchOptions_;

    std::unique_ptr<Board> board_;
    std::unique_ptr<IterativeSearcher> searcher_;

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



    void startSearch(const SearchOptions &options);
    void stopSearch();
    void iterationCallback(const SearchResult &result);
};
