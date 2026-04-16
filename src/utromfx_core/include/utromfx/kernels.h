#pragma once
#include <cmath>
#include <algorithm>

namespace utromfx {

template<size_t InDim, size_t OutDim>
struct LinearLayer {
    alignas(16) float weights[OutDim][InDim];
    alignas(16) float bias[OutDim];

    inline void forward(const float* input, float* output) const {
        for (size_t i = 0; i < OutDim; ++i) {
            float sum = bias[i];
            for (size_t j = 0; j < InDim; ++j) {
                sum += input[j] * weights[i][j];
            }
            output[i] = sum;
        }
    }
};

template<size_t Dim>
inline void relu(float* data) {
    for (size_t i = 0; i < Dim; ++i) {
        data[i] = (data[i] > 0.0f) ? data[i] : 0.0f;
    }
}

template<size_t Dim>
inline void tanh(float* data) {
    for (size_t i = 0; i < Dim; ++i) {
        data[i] = std::tanh(data[i]);
    }
}

} // namespace utromfx