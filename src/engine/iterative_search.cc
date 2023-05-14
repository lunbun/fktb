#include "iterative_search.h"

#include <memory>
#include <chrono>
#include <iostream>

#include "search.h"

IterativeSearcher::IterativeSearcher(const Board &board) : board_(board.copy()) { }

void IterativeSearcher::startSearch() {
    int32_t depth = 1;

    std::unique_ptr<FixedDepthSearcher> previousIteration, currentIteration;

    while (true) {
        currentIteration = std::make_unique<FixedDepthSearcher>(this->board_, depth, previousIteration.get());

        auto start = std::chrono::high_resolution_clock::now();

        FixedDepthSearchResult result = currentIteration->searchRoot();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "depth " << depth << " " << result.move->debugName() << " " << result.score << " "
                  << result.nodeCount << " " << result.transpositionHits << " " << duration << std::endl;

        previousIteration = std::move(currentIteration);

        ++depth;
    }
}
