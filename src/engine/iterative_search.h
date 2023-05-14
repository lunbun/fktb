#pragma once

#include "search.h"

class IterativeSearcher {
public:
    explicit IterativeSearcher(const Board &board);

    void startSearch();

private:
    Board board_;
};
