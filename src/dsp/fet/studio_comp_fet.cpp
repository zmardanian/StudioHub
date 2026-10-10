#include "dsp/fet/studio_comp_fet.hpp"

#include <algorithm>
#include <cmath>

namespace studiohub::dsp {
namespace {
constexpr double kEpsilon = 1.0e-12;
constexpr double kMinDb = -120.0;
constexpr double kParameterSmoothingSeconds = 0.010;

double to_linear(double db) noexcept
{
    return std::exp(db * 0.1151292546497022842); // ln(10) / 20
}

double to_db(double linear) noexcept
{
    return 20.0 * std::log10(std::max(linear, kEpsilon));
}
} // namespace

StudioCompFET::StudioCompFET(double sample_rate) noexcept
    : sample_rate_(48000.0),
      target_{},
      current_{-18.0, 4.0, 0.5, 80.0, 0.0, 0.0, 0.0, 1.0},
      target_values_{-18.0, 4.0, 0.5, 80.0, 0.0, 0.0, 0.0, 1.0},
      parameter_ramp_remaining_(0),
      envelope_(0.0),
      gain_reduction_db_(0.0)
{
    prepare(sample_rate);
}

void StudioCompFET::prepare(double sample_rate) noexcept
{
    sample_rate_ = std::isfinite(sample_rate)
        ? std::clamp(sample_rate, 8000.0, 768000.0)
        : 48000.0;
    parameter_ramp_remaining_ = 0;
    current_ = target_values_;
    reset();
}

void StudioCompFET::reset() noexcept
{
    envelope_ = 0.0;
    gain_reduction_db_ = 0.0;
}

CompressorParams StudioCompFET::sanitize(
    const CompressorParams& params) noexcept
{
    const auto finite_or = [](double value, double fallback) noexcept {
        return std::isfinite(value) ? value : fallback;
    };
    return {
        std::clamp(finite_or(params.threshold_db, -18.0), -60.0, 0.0),
        std::clamp(finite_or(params.ratio, 4.0), 1.0, 20.0),
        std::clamp(finite_or(params.attack_ms, 0.5), 0.05, 100.0),
        std::clamp(finite_or(params.release_ms, 80.0), 5.0, 2000.0),
        std::clamp(finite_or(params.input_gain_db, 0.0), -24.0, 24.0),
        std::clamp(finite_or(params.output_gain_db, 0.0), -24.0, 24.0),
        params.bypass,
        std::clamp(finite_or(params.knee_db, 0.0), 0.0, 12.0)
    };
}

void StudioCompFET::set_params(const CompressorParams& params) noexcept
{
    target_ = sanitize(params);
    target_values_ = {
        target_.threshold_db,
        target_.ratio,
        target_.attack_ms,
        target_.release_ms,
        target_.input_gain_db,
        target_.output_gain_db,
        target_.knee_db,
        target_.bypass ? 0.0 : 1.0
    };
    parameter_ramp_remaining_ = static_cast<std::size_t>(
        std::max(1.0, sample_rate_ * kParameterSmoothingSeconds));
}

const CompressorParams& StudioCompFET::target_params() const noexcept
{
    return target_;
}

void StudioCompFET::advance_parameters() noexcept
{
    if (parameter_ramp_remaining_ == 0)
        return;
    const double divisor = static_cast<double>(parameter_ramp_remaining_);
    current_.threshold_db += (target_values_.threshold_db - current_.threshold_db) / divisor;
    current_.ratio += (target_values_.ratio - current_.ratio) / divisor;
    current_.attack_ms += (target_values_.attack_ms - current_.attack_ms) / divisor;
    current_.release_ms += (target_values_.release_ms - current_.release_ms) / divisor;
    current_.input_gain_db += (target_values_.input_gain_db - current_.input_gain_db) / divisor;
    current_.output_gain_db += (target_values_.output_gain_db - current_.output_gain_db) / divisor;
    current_.knee_db += (target_values_.knee_db - current_.knee_db) / divisor;
    current_.wet += (target_values_.wet - current_.wet) / divisor;
    --parameter_ramp_remaining_;
}

void StudioCompFET::process(float* const* channels,
                            std::size_t num_channels,
                            std::size_t num_samples) noexcept
{
    if (!channels || num_channels == 0 || num_samples == 0)
        return;

    for (std::size_t n = 0; n < num_samples; ++n) {
        advance_parameters();
        const double input_gain = to_linear(current_.input_gain_db);
        const double output_gain = to_linear(current_.output_gain_db);
        const double attack = std::exp(-1.0 /
            (0.001 * current_.attack_ms * sample_rate_));
        const double release = std::exp(-1.0 /
            (0.001 * current_.release_ms * sample_rate_));

        double detector = 0.0;
        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            if (!channels[ch])
                continue;
            const double input = std::isfinite(channels[ch][n])
                ? static_cast<double>(channels[ch][n]) : 0.0;
            detector = std::max(detector, std::abs(input) * input_gain);
        }
        if (!std::isfinite(detector))
            detector = 0.0;

        const double detector_coefficient = detector > envelope_ ? attack : release;
        envelope_ = detector_coefficient * envelope_ +
                    (1.0 - detector_coefficient) * detector;
        if (!std::isfinite(envelope_))
            envelope_ = 0.0;

        const double level_db = std::max(kMinDb, to_db(envelope_));
        const double over_db = level_db - current_.threshold_db;
        const double slope = 1.0 - 1.0 / current_.ratio;
        const double knee_half_db = 0.5 * current_.knee_db;
        if (current_.knee_db <= 1.0e-9 || over_db >= knee_half_db) {
            gain_reduction_db_ = std::max(0.0, over_db) * slope;
        } else if (over_db <= -knee_half_db) {
            gain_reduction_db_ = 0.0;
        } else {
            const double knee_position = over_db + knee_half_db;
            gain_reduction_db_ = slope * knee_position * knee_position /
                                 (2.0 * current_.knee_db);
        }
        if (!std::isfinite(gain_reduction_db_))
            gain_reduction_db_ = 0.0;
        const double compression_gain = to_linear(-gain_reduction_db_);

        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            if (!channels[ch])
                continue;
            const double dry = std::isfinite(channels[ch][n])
                ? static_cast<double>(channels[ch][n]) : 0.0;
            const double wet = dry * input_gain * compression_gain * output_gain;
            const double output = dry * (1.0 - current_.wet) + wet * current_.wet;
            channels[ch][n] = static_cast<float>(
                std::isfinite(output) ? output : 0.0);
        }
    }
}

double StudioCompFET::last_gain_reduction_db() const noexcept
{
    return gain_reduction_db_;
}

} // namespace studiohub::dsp
