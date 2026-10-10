#pragma once

#include <cmath>
#include <cstddef>

namespace studiohub::analysis {

struct BasicMeasurement {
    double peak = 0.0;
    double rms = 0.0;
    double dc = 0.0;
};

class BasicAnalyzer {
public:
    void reset() noexcept {
        peak_ = 0.0;
        sum_sq_ = 0.0;
        sum_ = 0.0;
        count_ = 0;
    }

    void push(double sample) noexcept {
        if (!std::isfinite(sample)) return;
        peak_ = sample > 0.0 ? (sample > peak_ ? sample : peak_)
                             : (-sample > peak_ ? -sample : peak_);
        sum_sq_ += sample * sample;
        sum_ += sample;
        ++count_;
    }

    [[nodiscard]] BasicMeasurement result() const noexcept {
        if (count_ == 0) return {};
        const double n = static_cast<double>(count_);
        return {peak_, std::sqrt(sum_sq_ / n), sum_ / n};
    }

private:
    double peak_ = 0.0;
    double sum_sq_ = 0.0;
    double sum_ = 0.0;
    std::size_t count_ = 0;
};

} // namespace studiohub::analysis
