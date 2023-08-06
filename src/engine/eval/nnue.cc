#include "nnue.h"

#include <cstdint>
#include <cmath>
#include <type_traits>

#include <immintrin.h>

#include "accumulator.h"
#include "embedded_network.h"
#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"
#include "engine/board/board.h"

namespace FKTB::NNUE {

// Weights are by default stored in row-major order, where the weights are indexed by weights[output][input].
alignas(ByteAlignment) int16_t Hidden1Weights[Hidden1Size * InputSize];
alignas(ByteAlignment) int16_t Hidden1Biases[Hidden1Size];
alignas(ByteAlignment) int16_t Hidden2Weights[Hidden2Size * Hidden1Size];
alignas(ByteAlignment) int32_t Hidden2Biases[Hidden2Size];
alignas(ByteAlignment) int16_t Hidden3Weights[Hidden3Size * Hidden2Size];
alignas(ByteAlignment) int32_t Hidden3Biases[Hidden3Size];
alignas(ByteAlignment) int16_t OutputWeights[OutputSize * Hidden3Size];
alignas(ByteAlignment) int32_t OutputBiases[OutputSize];

// Weights stored in column-major order, where the weights are indexed by weights[input][output].
alignas(ByteAlignment) int16_t Hidden1WeightsColumnMajor[InputSize * Hidden1Size];

namespace {

// Loads parameters as floats from the given source, and quantizes them to the given type and scale. Also moves the source pointer
// forward by the given size.
template<typename T>
void loadQuantizedParameters(const float *&src, T *dst, uint32_t size, uint32_t scale) {
    static_assert(std::is_integral_v<T>, "T must be an integral type to load quantized parameters.");

    float min = static_cast<float>(std::numeric_limits<T>::min()) / static_cast<float>(scale);
    float max = static_cast<float>(std::numeric_limits<T>::max()) / static_cast<float>(scale);
    for (uint32_t i = 0; i < size; ++i) {
        float value = src[i];
        if (value < min || value > max) {
            throw std::runtime_error("Quantized parameter out of range.");
        }

        dst[i] = static_cast<T>(std::lround(value * static_cast<float>(scale)));
    }

    src += size;
}

template<typename T>
void transpose(const T *src, T *dst, uint32_t srcRows, uint32_t srcCols) {
    for (uint32_t i = 0; i < srcRows; ++i) {
        for (uint32_t j = 0; j < srcCols; ++j) {
            dst[i * srcCols + j] = src[j * srcRows + i];
        }
    }
}

// Loads the neural network from the data embedded in the executable.
INLINE void load() {
    auto data = reinterpret_cast<const float *>(NNUE_NETWORK_START);

    // The weights are stored by default in row-major order, where the weights are indexed by weights[output][input].
    //
    // First layer weights need to be scaled by InputsScale, not WeightsScale since they are used by the accumulator (which does
    // not have scaled inputs).
    loadQuantizedParameters(data, Hidden1Weights, Hidden1Size * InputSize, InputsScale);
    loadQuantizedParameters(data, Hidden1Biases, Hidden1Size, InputsScale);
    loadQuantizedParameters(data, Hidden2Weights, Hidden2Size * Hidden1Size, WeightsScale);
    loadQuantizedParameters(data, Hidden2Biases, Hidden2Size, InputsScale);
    loadQuantizedParameters(data, Hidden3Weights, Hidden3Size * Hidden2Size, WeightsScale);
    loadQuantizedParameters(data, Hidden3Biases, Hidden3Size, InputsScale);
    loadQuantizedParameters(data, OutputWeights, OutputSize * Hidden3Size, WeightsScale);
    loadQuantizedParameters(data, OutputBiases, OutputSize, InputsScale);

    // Transpose the row-major weights to column-major.
    transpose(Hidden1Weights, Hidden1WeightsColumnMajor, InputSize, Hidden1Size);
}



// Horizontally adds 4 __m256i vectors of 32-bit signed integers, producing 4 32-bit signed integers in a __m128i.
// Taken from https://github.com/glinscott/nnue-pytorch/blob/master/docs/nnue.md#m256_haddx4
INLINE __m128i hadd32x4(__m256i a, __m256i b, __m256i c, __m256i d) {
    a = _mm256_hadd_epi32(a, b);
    c = _mm256_hadd_epi32(c, d);
    a = _mm256_hadd_epi32(a, c);
    return _mm_add_epi32(_mm256_castsi256_si128(a),  _mm256_extracti128_si256(a, 1));
}

// Horizontally adds the 32-bit signed integers in x, producing a 32-bit signed integer.
INLINE int32_t hadd32(__m128i x) {
    x = _mm_hadd_epi32(x, x);
    return _mm_extract_epi32(x, 0) + _mm_extract_epi32(x, 1);
}

// Loads the weights from the given pointer, multiplies them with the given input, and accumulates the product into the given
// accumulator vector.
INLINE void accumulate(__m256i &accum, __m256i input, const int16_t *weights) {
    __m256i weightsVector = _mm256_load_si256(reinterpret_cast<const __m256i *>(weights));
    __m256i product = _mm256_madd_epi16(input, weightsVector);
    accum = _mm256_add_epi32(accum, product);
}

// Forward propagates a single ReLU layer of the neural network.
template<uint32_t InputSize, uint32_t OutputSize>
INLINE void forwardLayer(const int16_t *input, const int16_t *weights, const int32_t *biases, int16_t *output) {
    static_assert(InputSize % RegisterWidth16 == 0, "Input size must be a multiple of RegisterWidth16.");
    static_assert(OutputSize % 4 == 0, "Output size must be a multiple of 4.");

    assert(reinterpret_cast<uintptr_t>(input) % ByteAlignment == 0 && "input must be aligned to ByteAlignment.");
    assert(reinterpret_cast<uintptr_t>(weights) % ByteAlignment == 0 && "weights must be aligned to ByteAlignment.");

    for (uint32_t i = 0; i < OutputSize; i += 4) {
        uint32_t offset0 = (i + 0) * InputSize;
        uint32_t offset1 = (i + 1) * InputSize;
        uint32_t offset2 = (i + 2) * InputSize;
        uint32_t offset3 = (i + 3) * InputSize;

        __m256i accum0 = _mm256_setzero_si256();
        __m256i accum1 = _mm256_setzero_si256();
        __m256i accum2 = _mm256_setzero_si256();
        __m256i accum3 = _mm256_setzero_si256();

        for (uint32_t j = 0; j < InputSize; j += RegisterWidth16) {
            __m256i inputVector = _mm256_load_si256(reinterpret_cast<const __m256i *>(input + j));

            accumulate(accum0, inputVector, weights + offset0 + j);
            accumulate(accum1, inputVector, weights + offset1 + j);
            accumulate(accum2, inputVector, weights + offset2 + j);
            accumulate(accum3, inputVector, weights + offset3 + j);
        }

        __m128i outputVector = hadd32x4(accum0, accum1, accum2, accum3);

        // Account for the fact that the weights are scaled by WeightsScale.
        outputVector = _mm_srai_epi32(outputVector, WeightsScaleLog2);

        __m128i biasesVector = _mm_load_si128(reinterpret_cast<const __m128i *>(biases + i));
        outputVector = _mm_add_epi32(outputVector, biasesVector);

        // Apply ReLU and convert to 16-bit integers.
        outputVector = _mm_max_epi32(outputVector, _mm_setzero_si128());
        outputVector = _mm_packs_epi32(outputVector, outputVector);

        // Store the lower 64 bits of the output vector (the upper 64 bits are unused since we converted from 32-bit to 16-bit
        // integers).
        _mm_storel_epi64(reinterpret_cast<__m128i *>(output + i), outputVector);
    }
}

// Forward propagates the output layer of the neural network. Hidden3 must be of size Hidden3Size.
INLINE int32_t forwardOutput(const int16_t *hidden3) {
    static_assert(Hidden3Size == RegisterWidth16_128, "Hidden3Size must be RegisterWidth16_128.");
    static_assert(OutputSize == 1, "OutputSize must be 1.");

    assert(reinterpret_cast<uintptr_t>(hidden3) % ByteAlignment == 0 && "hidden3 must be aligned to ByteAlignment.");
    assert(reinterpret_cast<uintptr_t>(OutputWeights) % ByteAlignment == 0 && "OutputWeights must be aligned to ByteAlignment.");

    // The entire output layer can be done with just a few 128-bit vector instructions!
    __m128i input = _mm_load_si128(reinterpret_cast<const __m128i *>(hidden3));
    __m128i weights = _mm_load_si128(reinterpret_cast<const __m128i *>(OutputWeights));

    __m128i accum = _mm_madd_epi16(input, weights);

    int32_t sum = hadd32(accum);

    // Account for the fact that the weights are scaled by WeightsScale.
    sum >>= WeightsScaleLog2;

    return sum + OutputBiases[0];
}

// Forward propagates the neural network. Hidden1 must be of size Hidden1Size.
INLINE int32_t forward(const int16_t *hidden1) {
    alignas(ByteAlignment) int16_t hidden2[Hidden2Size];
    alignas(ByteAlignment) int16_t hidden3[Hidden3Size];

    forwardLayer<Hidden1Size, Hidden2Size>(hidden1, Hidden2Weights, Hidden2Biases, hidden2);
    forwardLayer<Hidden2Size, Hidden3Size>(hidden2, Hidden3Weights, Hidden3Biases, hidden3);
    return forwardOutput(hidden3);
}

} // namespace

// Loads the neural network.
void init() {
    load();
}

// Evaluates the given board position from the perspective of the given side.
int32_t evaluate(Color side, const NNUE::Accumulator &accumulator) {
    alignas(ByteAlignment) int16_t hidden1[InputSize];

    // Apply ReLU to the accumulator.
    accumulator.relu(side, hidden1);

    // Evaluate the board.
    int32_t eval = forward(hidden1);

    // Convert the evaluation to centipawns.
    return (eval * 100) >> InputsScaleLog2;
}

} // namespace FKTB::NNUE
