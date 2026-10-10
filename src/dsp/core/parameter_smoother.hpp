#pragma once

#include <cstddef>

namespace studiohub::dsp {

class LinearParameterSmoother {
public:
    explicit LinearParameterSmoother(double sample_rate = 48000.0) noexcept;

    void prepare(double sample_rate) noexcept;

    void reset(double value) noexcept;

    void set_target(double target, double time_seconds) noexcept;

    [[nodiscard]]
    double get_next_value() noexcept;

    [[nodiscard]]
    double current_value() const noexcept;

    [[nodiscard]]
    double target_value() const noexcept;

    [[nodiscard]]
    bool is_smoothing() const noexcept;

private:
    double sample_rate_;
    double current_;
    double target_;
    double increment_;
    std::size_t remaining_samples_;
};

} // namespace studiohub::dsp
