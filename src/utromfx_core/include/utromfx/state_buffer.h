#pragma once
#include <array>

namespace utromfx {

/**
 * @brief Stack-allocated history buffer for Sim-to-Real parity.
 */
template<size_t FeatureDim, size_t HistoryLen>
class ReflexBuffer {
private:
    std::array<float, FeatureDim * HistoryLen> buffer;
    size_t head = 0;

public:
    // Pushes new observation and maintains the rolling window
    void push(const float* new_obs) {
        size_t offset = head * FeatureDim;
        for (size_t i = 0; i < FeatureDim; ++i) {
            buffer[offset + i] = new_obs[i];
        }
        head = (head + 1) % HistoryLen;
    }

    // Flattened view for the Neural Network input
    void flatten(float* out) const {
        // Implementation ensures the oldest frame is always at index 0
        for (size_t i = 0; i < HistoryLen; ++i) {
            size_t idx = (head + i) % HistoryLen;
            std::copy(buffer.begin() + idx * FeatureDim, 
                      buffer.begin() + (idx + 1) * FeatureDim, 
                      out + i * FeatureDim);
        }
    }
};

} // namespace utromfx