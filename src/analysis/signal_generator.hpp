#pragma once

#include <cstddef>
#include <numbers>

namespace studiohub::analysis {

class SineGenerator {
public:
    explicit SineGenerator(double sample_rate = 48000.0) noexcept;

    void prepare(double sample_rate) noexcept;

    void reset(double phase = 0.0) noexcept;

    void set_frequency(double frequency_hz) noexcept;

    void set_amplitude(double amplitude) noexcept;

    [[nodiscard]]
    double next_sample() noexcept;

    [[nodiscard]]
    double phase() const noexcept;

    [[nodiscard]]
    double frequency() const noexcept;

    [[nodiscard]]
    double amplitude() const noexcept;

private:
    double sample_rate_;
    double frequency_hz_;
    double amplitude_;
    double phase_;
    double phase_increment_;
};

} // namespace studiohub::analysis
