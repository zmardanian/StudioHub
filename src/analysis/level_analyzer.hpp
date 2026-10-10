#pragma once

#include <cstddef>

namespace studiohub::analysis {

struct LevelMeasurement {
    double peak = 0.0;
    double rms = 0.0;
    double dc = 0.0;

    [[nodiscard]]
    double peak_dbfs() const noexcept;

    [[nodiscard]]
    double rms_dbfs() const noexcept;
};

class LevelAnalyzer {
public:
    void reset() noexcept;

    void process(
        const float* samples,
        std::size_t count) noexcept;

    void process(
        const double* samples,
        std::size_t count) noexcept;

    [[nodiscard]]
    LevelMeasurement measurement() const noexcept;

private:
    double peak_ = 0.0;
    double sum_square_ = 0.0;
    double sum_ = 0.0;
    std::size_t sample_count_ = 0;
};

} // namespace studiohub::analysis
