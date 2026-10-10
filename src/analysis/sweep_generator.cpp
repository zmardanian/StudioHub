#include "analysis/sweep_generator.hpp"

#include <algorithm>
#include <numbers>

namespace studiohub::analysis {

LogSweepGenerator::LogSweepGenerator(
    const double sample_rate) noexcept
    : sample_rate_(sample_rate > 0.0 ? sample_rate : 48000.0),
      start_frequency_hz_(20.0),
      end_frequency_hz_(20000.0),
      duration_seconds_(10.0),
      amplitude_(1.0),
      total_samples_(0.0),
      sample_position_(0.0),
      phase_(0.0),
      phase_increment_(0.0)
{
    configure(
        start_frequency_hz_,
        end_frequency_hz_,
        duration_seconds_,
        amplitude_
    );
}

void LogSweepGenerator::prepare(
    const double sample_rate) noexcept
{
    if (std::isfinite(sample_rate) && sample_rate > 0.0) {
        sample_rate_ = sample_rate;

        configure(
            start_frequency_hz_,
            end_frequency_hz_,
            duration_seconds_,
            amplitude_
        );
    }
}

void LogSweepGenerator::reset() noexcept
{
    sample_position_ = 0.0;
    phase_ = 0.0;
    phase_increment_ =
        2.0 * std::numbers::pi *
        start_frequency_hz_ /
        sample_rate_;
}

void LogSweepGenerator::configure(
    const double start_frequency_hz,
    const double end_frequency_hz,
    const double duration_seconds,
    const double amplitude) noexcept
{
    if (!std::isfinite(start_frequency_hz) ||
        start_frequency_hz <= 0.0) {
        start_frequency_hz_ = 20.0;
    } else {
        start_frequency_hz_ = start_frequency_hz;
    }

    if (!std::isfinite(end_frequency_hz) ||
        end_frequency_hz <= 0.0) {
        end_frequency_hz_ = 20000.0;
    } else {
        end_frequency_hz_ = end_frequency_hz;
    }

    if (end_frequency_hz_ <= start_frequency_hz_) {
        end_frequency_hz_ = start_frequency_hz_ * 2.0;
    }

    duration_seconds_ =
        std::isfinite(duration_seconds) &&
        duration_seconds > 0.0
            ? duration_seconds
            : 1.0;

    amplitude_ =
        std::isfinite(amplitude)
            ? amplitude
            : 0.0;

    total_samples_ =
        std::max(
            1.0,
            std::ceil(
                duration_seconds_ * sample_rate_
            )
        );

    reset();
}

double LogSweepGenerator::next_sample() noexcept
{
    if (finished()) {
        return 0.0;
    }

    const double progress =
        sample_position_ /
        total_samples_;

    const double ratio =
        end_frequency_hz_ /
        start_frequency_hz_;

    const double frequency =
        start_frequency_hz_ *
        std::pow(ratio, progress);

    phase_increment_ =
        2.0 * std::numbers::pi *
        frequency /
        sample_rate_;

    const double value =
        amplitude_ *
        std::sin(phase_);

    phase_ += phase_increment_;

    phase_ =
        std::fmod(
            phase_,
            2.0 * std::numbers::pi
        );

    if (phase_ < 0.0) {
        phase_ += 2.0 * std::numbers::pi;
    }

    sample_position_ += 1.0;

    return value;
}

bool LogSweepGenerator::finished() const noexcept
{
    return sample_position_ >= total_samples_;
}

double LogSweepGenerator::progress() const noexcept
{
    if (total_samples_ <= 0.0) {
        return 1.0;
    }

    return std::clamp(
        sample_position_ / total_samples_,
        0.0,
        1.0
    );
}

double LogSweepGenerator::current_frequency() const noexcept
{
    const double ratio =
        end_frequency_hz_ /
        start_frequency_hz_;

    return start_frequency_hz_ *
           std::pow(
               ratio,
               progress()
           );
}

double LogSweepGenerator::sample_rate() const noexcept
{
    return sample_rate_;
}

} // namespace studiohub::analysis
