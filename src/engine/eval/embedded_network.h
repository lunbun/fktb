#pragma once

#include <cstdint>

// The neural network is embedded into the executable. This is done by having CMake create a .o file from the network, and linking
// it into the executable. The symbols to link to the network are in the NNUE_NETWORK_* preprocessor definitions.
#if !defined(NNUE_NETWORK_START) || !defined(NNUE_NETWORK_END) || !defined(NNUE_NETWORK_SIZE)
static_assert(false, "Embedded NNUE network not found");
#endif

extern const float NNUE_NETWORK_START[];
extern const float NNUE_NETWORK_END[];
extern const size_t NNUE_NETWORK_SIZE;
