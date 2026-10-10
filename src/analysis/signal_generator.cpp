#include "analysis/signal_generator.hpp"

#include <cmath>

namespace studiohub::analysis {

SineGenerator::SineGenerator(
    const double sample_rate) noexcept
    : sample_rate_(sample_rate > 0.0 ? sample_rate : 48000.0),
      frequency_hz_(1000.0),
      amplitude_(1.0),
      phase_(0.0),
      phase_increment_(0.0)
{
    set_frequency(frequency_hz_);
}

void SineGenerator::prepare(
    const double sample_rate) noexcept
{
    if (std::isfinite(sample_rate) && sample_rate > 0.0) {
        sample_rate_ = sample_rate;
        set_frequency(frequency_hz_);
    }
}

void SineGenerator::reset(
    const double phase) noexcept
{
    phase_ = std::isfinite(phase) ? phase : 0.0;
}

void SineGenerator::set_frequency(
    const double frequency_hz) noexcept
{
    if (!std::isfinite(frequency_hz)) {
        frequency_hz_ = 0.0;
    } else {
        frequency_hz_ = frequency_hz;
    }

    phase_increment_ =
        (frequency_hz_ / sample_rate_) *
        (2.0 * std::numbers::pi);
}

void SineGenerator::set_amplitude(
    const double amplitude) noexcept
{
    amplitude_ = std::isfinite(amplitude) ? amplitude : 0.0;
}

double SineGenerator::next_sample() noexcept
{
    const double value = amplitude_ * std::sin(phase_);

    phase_ += phase_increment_;

    phase_ = std::fmod(
        phase_,
        2.0 * std::numbers::pi
    );

    if (phase_ < 0.0) {
        phase_ += 2.0 * std::numbers::pi;
    }

    return value;
}

double SineGenerator::phase() const noexcept
{
    return phase_;
}

double SineGenerator::frequency() const noexcept
{
    return frequency_hz_;
}

double SineGenerator::amplitude() const noexcept
{
    return amplitude_;
}

} // namespace studiohub::analysis
