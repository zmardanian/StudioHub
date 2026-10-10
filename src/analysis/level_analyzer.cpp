#include "analysis/level_analyzer.hpp"

#include <algorithm>
#include <cmath>

namespace studiohub::analysis {

namespace {

constexpr double kDbFloor = -600.0;

[[nodiscard]]
double amplitude_to_dbfs(
    const double amplitude) noexcept
{
    if (!(amplitude > 1.0e-30) ||
        !std::isfinite(amplitude)) {
        return kDbFloor;
    }

    return 20.0 * std::log10(amplitude);
}

} // namespace

double LevelMeasurement::peak_dbfs() const noexcept
{
    return amplitude_to_dbfs(peak);
}

double LevelMeasurement::rms_dbfs() const noexcept
{
    return amplitude_to_dbfs(rms);
}

void LevelAnalyzer::reset() noexcept
{
    peak_ = 0.0;
    sum_square_ = 0.0;
    sum_ = 0.0;
    sample_count_ = 0;
}

void LevelAnalyzer::process(
    const float* const samples,
    const std::size_t count) noexcept
{
    if (samples == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < count; ++i) {
        const double sample =
            static_cast<double>(samples[i]);

        if (!std::isfinite(sample)) {
            continue;
        }

        const double magnitude =
            std::abs(sample);

        peak_ =
            std::max(peak_, magnitude);

        sum_square_ +=
            sample * sample;

        sum_ += sample;

        ++sample_count_;
    }
}

void LevelAnalyzer::process(
    const double* const samples,
    const std::size_t count) noexcept
{
    if (samples == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < count; ++i) {
        const double sample = samples[i];

        if (!std::isfinite(sample)) {
            continue;
        }

        const double magnitude =
            std::abs(sample);

        peak_ =
            std::max(peak_, magnitude);

        sum_square_ +=
            sample * sample;

        sum_ += sample;

        ++sample_count_;
    }
}

LevelMeasurement LevelAnalyzer::measurement() const noexcept
{
    if (sample_count_ == 0) {
        return {};
    }

    const double count =
        static_cast<double>(sample_count_);

    return {
        peak_,
        std::sqrt(sum_square_ / count),
        sum_ / count
    };
}

} // namespace studiohub::analysis
