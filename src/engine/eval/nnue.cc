#include "nnue.h"

#include <cstdint>
#include <array>
#include <cmath>

#include <immintrin.h>

#include "embedded_network.h"
#include "engine/inline.h"
#include "engine/board/color.h"
#include "engine/board/piece.h"
#include "engine/board/square.h"
#include "engine/board/board.h"

namespace FKTB {

namespace {

// TODO: Add a NNUE accumulator.
// TODO: Network quantization for more performance.
constexpr uint32_t InputSize = 768;
constexpr uint32_t Hidden1Size = 512;
constexpr uint32_t Hidden2Size = 64;
constexpr uint32_t Hidden3Size = 32;
constexpr uint32_t OutputSize = 1;

constexpr const float *Hidden1Weights = NNUE_NETWORK_START;
constexpr const float *Hidden1Biases = Hidden1Weights + Hidden1Size * InputSize;
constexpr const float *Hidden2Weights = Hidden1Biases + Hidden1Size;
constexpr const float *Hidden2Biases = Hidden2Weights + Hidden2Size * Hidden1Size;
constexpr const float *Hidden3Weights = Hidden2Biases + Hidden2Size;
constexpr const float *Hidden3Biases = Hidden3Weights + Hidden3Size * Hidden2Size;
constexpr const float *OutputWeights = Hidden3Biases + Hidden3Size;
constexpr const float *OutputBiases = OutputWeights + OutputSize * Hidden3Size;



// Returns the index in the input tensor for the given piece and square.
INLINE uint32_t inputIndex(PieceType type, Square square, bool flipVertically) {
    if (flipVertically) {
        square = { square.file(), static_cast<uint8_t>(7 - square.rank()) };
    }

    return static_cast<uint32_t>(type) * 64 + static_cast<uint32_t>(square);
}

// Transforms an individual side of the board into a neural network input.
template<Color Side, bool FlipVertically>
INLINE void transformSide(const Board &board, float *input) {
    // Pieces
    for (PieceType type = PieceType::Pawn; type <= PieceType::Queen; type = static_cast<PieceType>(type + 1)) {
        Bitboard pieces = board.bitboard({ Side, type });
        for (Square piece : pieces) {
            input[inputIndex(type, piece, FlipVertically)] = 1.0f;
        }
    }

    // King
    Square king = board.king(Side);
    input[inputIndex(PieceType::King, king, FlipVertically)] = 1.0f;
}

// Transforms the board into a neural network input. Input must be of size InputSize, and must be zeroed.
template<Color Side>
INLINE void transform(const Board &board, float *input) {
    // Number of inputs for each side.
    constexpr uint32_t InputSideSize = 384;

    // Need to flip the board vertically if Side is black (neural network does not have a concept of side-to-move, it expects the
    // board to be from side-to-move's perspective).
    constexpr bool FlipVertically = (Side == Color::Black);

    transformSide<Side, FlipVertically>(board, input);
    transformSide<~Side, FlipVertically>(board, input + InputSideSize);
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

// Forward propagates the neural network. Input must be of size InputSize.
INLINE float forward(const float *input) {
    float hidden1[Hidden1Size];
    float hidden2[Hidden2Size];
    float hidden3[Hidden3Size];
    float output[OutputSize];

    forwardLayer<InputSize, Hidden1Size, relu>(input, Hidden1Weights, Hidden1Biases, hidden1);
    forwardLayer<Hidden1Size, Hidden2Size, relu>(hidden1, Hidden2Weights, Hidden2Biases, hidden2);
    forwardLayer<Hidden2Size, Hidden3Size, relu>(hidden2, Hidden3Weights, Hidden3Biases, hidden3);
    forwardLayer<Hidden3Size, OutputSize, linear>(hidden3, OutputWeights, OutputBiases, output);

    return output[0];
}

} // namespace

// Evaluates the given board position from the perspective of the given side.
template<Color Side>
int32_t NNUE::evaluate(const Board &board) {
    // Transform the board into a neural network input.
    std::array<float, InputSize> input = { 0 };
    transform<Side>(board, input.data());

    // Evaluate the board.
    float eval = forward(input.data());

    // Convert the evaluation to centipawns.
    return std::lround(eval * 100.0f);
}

template int32_t NNUE::evaluate<Color::White>(const Board &board);
template int32_t NNUE::evaluate<Color::Black>(const Board &board);

} // namespace FKTB
