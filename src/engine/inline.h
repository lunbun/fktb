#pragma once

#ifndef INLINE

// Using always_inline over GCC's inline heuristic gives a significant performance boost
#define INLINE inline __attribute__((always_inline))

#endif
