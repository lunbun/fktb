#pragma once

#include <cstdint>
#include <memory>

#include "search.h"

class Engine {
public:
    Engine();
    explicit Engine(uint32_t threads);
    ~Engine();

    void startIterativeSearch(Board &board) const;

private:
    std::unique_ptr<SearchThreadManager> manager_;
};
