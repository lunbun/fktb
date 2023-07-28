#include "nnue.h"

#include <cstdint>
#include <array>
#include <cmath>

#include <immintrin.h>

#include "accumulator.h"
#include "embedded_network.h"
#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"
#include "engine/board/board.h"

namespace FKTB::NNUE {

const float *Hidden1Weights;
const float *Hidden1Biases;
const float *Hidden2Weights;
const float *Hidden2Biases;
const float *Hidden3Weights;
const float *Hidden3Biases;
const float *OutputWeights;
const float *OutputBiases;

namespace {

// TODO: Network quantization for more performance.

// Loads the neural network from the data embedded in the executable.
INLINE void load() {
    Hidden1Weights = reinterpret_cast<const float *>(NNUE_NETWORK_START);
    Hidden1Biases = Hidden1Weights + Hidden1Size * InputSize;
    Hidden2Weights = Hidden1Biases + Hidden1Size;
    Hidden2Biases = Hidden2Weights + Hidden2Size * Hidden1Size;
    Hidden3Weights = Hidden2Biases + Hidden2Size;
    Hidden3Biases = Hidden3Weights + Hidden3Size * Hidden2Size;
    OutputWeights = Hidden3Biases + Hidden3Size;
    OutputBiases = OutputWeights + OutputSize * Hidden3Size;
}



INLINE float horizontalSum(__m256 x) {
    // Taken from https://stackoverflow.com/a/23190168
    /* ( x3+x7, x2+x6, x1+x5, x0+x4 ) */
    const __m128 x128 = _mm_add_ps(_mm256_extractf128_ps(x, 1), _mm256_castps256_ps128(x));
    /* ( -, -, x1+x3+x5+x7, x0+x2+x4+x6 ) */
    const __m128 x64 = _mm_add_ps(x128, _mm_movehl_ps(x128, x128));
    /* ( -, -, -, x0+x1+x2+x3+x4+x5+x6+x7 ) */
    const __m128 x32 = _mm_add_ss(x64, _mm_shuffle_ps(x64, x64, 0x55));
    /* Conversion to float is a no-op on x86-64 */
    return _mm_cvtss_f32(x32);
}

INLINE float relu(float x) {
    return std::max(0.0f, x);
}

INLINE float linear(float x) {
    return x;
}

// Forward propagates a single layer of the neural network.
template<uint32_t InputSize, uint32_t OutputSize, float (*Activation)(float)>
INLINE void forwardLayer(const float *input, const float *weights, const float *biases, float *output) {
    static_assert(InputSize % 8 == 0, "Input size must be a multiple of 8.");

    for (uint32_t i = 0; i < OutputSize; ++i) {
        __m256 sumVector = _mm256_setzero_ps();

        // AVX
        for (uint32_t j = 0; j < InputSize; j += 8) {
            __m256 inputVector = _mm256_load_ps(input + j);
            __m256 weightVector = _mm256_load_ps(weights + i * InputSize + j);
            sumVector = _mm256_fmadd_ps(inputVector, weightVector, sumVector);
        }

        float sum = horizontalSum(sumVector);

        sum += biases[i];
        output[i] = Activation(sum);
    }
}

// Forward propagates the neural network. Hidden1 must be of size Hidden1Size.
INLINE float forward(const float *hidden1) {
    float hidden2[Hidden2Size];
    float hidden3[Hidden3Size];
    float output[OutputSize];

    forwardLayer<Hidden1Size, Hidden2Size, relu>(hidden1, Hidden2Weights, Hidden2Biases, hidden2);
    forwardLayer<Hidden2Size, Hidden3Size, relu>(hidden2, Hidden3Weights, Hidden3Biases, hidden3);
    forwardLayer<Hidden3Size, OutputSize, linear>(hidden3, OutputWeights, OutputBiases, output);

    return output[0];
}

} // namespace

// Loads the neural network.
void init() {
    load();
}

// Evaluates the given board position from the perspective of the given side.
int32_t evaluate(Color side, const NNUE::Accumulator &accumulator) {
    float hidden1[Hidden1Size];

    // Apply ReLU to the accumulator.
    const float *accumulatorData = accumulator[side];
    __m256 zero = _mm256_setzero_ps();
    for (uint32_t i = 0; i < Hidden1Size; i += 8) {
        __m256 vector = _mm256_load_ps(accumulatorData + i);
        vector = _mm256_max_ps(vector, zero);
        _mm256_store_ps(hidden1 + i, vector);
    }

    // Evaluate the board.
    float eval = forward(hidden1);

    // Convert the evaluation to centipawns.
    return std::lround(eval * 100.0f);
}

} // namespace FKTB::NNUE
