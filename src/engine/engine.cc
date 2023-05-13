#include "engine.h"

#include <cstdint>
#include <thread>

#include "search.h"

Engine::Engine() : Engine(std::thread::hardware_concurrency()) {}

Engine::Engine(uint32_t threads) {
    this->manager_ = std::make_unique<SearchThreadManager>(threads);
}

Engine::~Engine() = default;

void Engine::startIterativeSearch(Board &board) const {
    return this->manager_->startIterativeSearch(board);
}
