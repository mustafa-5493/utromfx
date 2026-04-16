#pragma once
#include <arm_neon.h>
#include <cstddef>
#include <algorithm>

namespace utromfx {

template<size_t InDim, size_t OutDim>
struct LinearLayer {
    // 16-byte alignment is critical for NEON performance
    alignas(16) float weights[OutDim][InDim];
    alignas(16) float bias[OutDim];

    inline void forward(const float* input, float* output) const {
        for (size_t i = 0; i < OutDim; ++i) {
            // Initialize accumulator with 4 lanes of 0
            float32x4_t acc_v = vdupq_n_f32(0.0f);
            
            size_t j = 0;
            // Main Vector Loop: Process 4 floats at a time
            for (; j <= InDim - 4; j += 4) {
                float32x4_t input_v = vld1q_f32(&input[j]);
                float32x4_t weight_v = vld1q_f32(&weights[i][j]);
                
                // Fused Multiply-Add: acc = acc + (input * weight)
                acc_v = vmlaq_f32(acc_v, input_v, weight_v);
            }

            // Horizontal Sum: Add the 4 lanes of the vector together
            float row_sum = vaddvq_f32(acc_v) + bias[i];

            // Tail Handler: Process remaining elements (if InDim % 4 != 0)
            for (; j < InDim; ++j) {
                row_sum += input[j] * weights[i][j];
            }

            output[i] = row_sum;
        }
    }
};

} // namespace utromfx