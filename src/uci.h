#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <optional>
#include <memory>
#include <functional>
#include <optional>
#include <fstream>

#include "engine/board/color.h"
#include "engine/board/board.h"
#include "engine/search/iterative_search.h"

namespace FKTB {

namespace UCI {

class TokenStream {
public:
    explicit TokenStream(const std::string &input);

    [[nodiscard]] bool isEnd() const { return this->index_ >= this->tokens_.size(); }
    [[nodiscard]] const std::string &next();
    [[nodiscard]] const std::string &peek() const { return this->tokens_[this->index_]; }

    // Reads the tokens until the given condition is met. Returns the tokens read (space separated), not including the token that
    // caused the condition to be met.
    [[nodiscard]] std::string readUntil(const std::function<bool(const std::string &)> &condition);

    [[nodiscard]] std::string readUntilEnd();
    [[nodiscard]] std::string readUntil(const std::string &token);

private:
    uint32_t index_;
    std::vector<std::string> tokens_;
};

struct TimeControl {
    ColorMap<std::optional<int32_t>> time;
    ColorMap<std::optional<int32_t>> increment;
};

struct SearchOptions {
    bool infinite = false;
    TimeControl timeControl;
    std::optional<uint16_t> depth;
    std::optional<uint64_t> nodes;
    std::optional<int32_t> moveTime;
};

class Handler {
public:
    Handler(std::string name, std::string author);
    ~Handler();

    [[noreturn]] void run();

private:
    class SearchStopThread;

    std::string name_, author_;

    std::unique_ptr<std::ofstream> logFile_;

    bool isSearching_ = false;
    std::optional<SearchOptions> searchOptions_;
    std::unique_ptr<SearchStopThread> searchStopThread_;

    std::unique_ptr<Board> board_;
    std::unique_ptr<IterativeSearcher> searcher_;

    void send(const std::string &message);
    void error(const std::string &message);
    std::string readInput();

    // Logs the message if a log file is set.
    void maybeLog(const std::string &message);

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

    void handleSetLogFile(const std::string &path);

    void handleTest(TokenStream &tokens);
    void handleTestMoveGen(TokenStream &tokens);
    void handleTestPrintFen(TokenStream &tokens);



    void startSearch(const SearchOptions &options);
    void stopSearch();
    void iterationCallback(const SearchResult &result);
};

} // namespace UCI

using UCIHandler = UCI::Handler;

} // namespace FKTB
