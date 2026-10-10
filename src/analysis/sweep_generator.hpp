#pragma once

#include <cmath>

namespace studiohub::analysis {

class LogSweepGenerator {
public:
    explicit LogSweepGenerator(
        double sample_rate = 48000.0) noexcept;

    void prepare(double sample_rate) noexcept;

    void reset() noexcept;

    void configure(
        double start_frequency_hz,
        double end_frequency_hz,
        double duration_seconds,
        double amplitude = 1.0) noexcept;

    [[nodiscard]]
    double next_sample() noexcept;

    [[nodiscard]]
    bool finished() const noexcept;

    [[nodiscard]]
    double progress() const noexcept;

    [[nodiscard]]
    double current_frequency() const noexcept;

    [[nodiscard]]
    double sample_rate() const noexcept;

private:
    double sample_rate_;
    double start_frequency_hz_;
    double end_frequency_hz_;
    double duration_seconds_;
    double amplitude_;

    double total_samples_;
    double sample_position_;
    double phase_;
    double phase_increment_;
};

} // namespace studiohub::analysis
