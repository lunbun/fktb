
#pragma once

#include <cstdint>
#include <cassert>
#include <x86intrin.h>

#include "inline.h"

namespace Intrinsics {
    // Bit scan forward: returns the index of the least significant bit.
    INLINE uint8_t bsf(uint64_t x) {
        assert(x != 0);
        return __builtin_ctzll(x);
    }

    // Bit scan reverse: returns the index of the most significant bit.
    INLINE uint8_t bsr(uint64_t x) {
        assert(x != 0);
        return 63 ^ __builtin_clzll(x);
    }

    // Population count: returns the number of set bits in the integer.
    INLINE uint8_t popcnt(uint64_t x) {
        return __builtin_popcountll(x);
    }

    // Byte swap: reverses the order of the bytes in the integer.
    INLINE uint64_t bswap(uint64_t x) {
        return __builtin_bswap64(x);
    }

    // Rotate left: rotates the bits of the integer to the left.
    INLINE uint64_t rol(uint64_t x, uint8_t shift) {
        return _rotl64(x, shift);
    }

    // Rotate right: rotates the bits of the integer to the right.
    INLINE uint64_t ror(uint64_t x, uint8_t shift) {
        return _rotr64(x, shift);
    }

    // Reset lowest set bit: sets the least significant bit to zero.
    // Equivalent to x & (x - 1).
    INLINE uint64_t blsr(uint64_t x) {
        return _blsr_u64(x);
    }

    // Extract lowest set bit: returns the least significant bit.
    // Equivalent to x & (-x).
    INLINE uint64_t blsi(uint64_t x) {
        return _blsi_u64(x);
    }

    // Parallel bits deposit: deposits bits from x into the mask.
    INLINE uint64_t pdep(uint64_t x, uint64_t mask) {
        return _pdep_u64(x, mask);
    }

    // Parallel bits extract: extracts bits from x using the mask.
    INLINE uint64_t pext(uint64_t x, uint64_t mask) {
        return _pext_u64(x, mask);
    }
}
