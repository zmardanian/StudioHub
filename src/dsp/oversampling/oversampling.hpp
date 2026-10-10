#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <utility>

namespace studiohub::dsp::oversampling {

enum class Factor : std::size_t { x1 = 1, x2 = 2, x4 = 4, x8 = 8, x16 = 16 };

// Allocation-free streaming oversampler. The filter is a fixed, linear-phase,
// Blackman-windowed sinc low-pass prototype. State is owned by the instance and
// must be prepared/reset before entering the audio thread. process() itself
// performs no allocation or I/O.
class Oversampler {
public:
    static constexpr std::size_t kMaxTaps = 511U;
    static constexpr std::size_t kMaxFactor = 16U;

    explicit Oversampler(Factor factor = Factor::x1) noexcept : factor_(factor) {
        design_filter();
    }

    void reset() noexcept {
        input_history_.fill(0.0);
        output_history_.fill(0.0);
        input_write_ = 0U;
        output_write_ = 0U;
    }

    void set_factor(Factor factor) noexcept {
        factor_ = factor;
        design_filter();
        reset();
    }

    [[nodiscard]] Factor factor() const noexcept { return factor_; }
    [[nodiscard]] std::size_t factor_value() const noexcept { return static_cast<std::size_t>(factor_); }
    [[nodiscard]] std::size_t high_rate_samples_per_input() const noexcept { return factor_value(); }

    // Process one input sample. The callback is invoked exactly factor_value()
    // times at the oversampled rate. Its return value is the nonlinear/high-rate
    // processed sample. The returned value is the decimated output sample.
    template <typename Processor>
    [[nodiscard]] double process(double input, Processor&& processor) noexcept {
        const std::size_t factor = factor_value();
        if (factor == 1U) return processor(input);

        input_history_[input_write_] = input;
        input_write_ = (input_write_ + 1U) % kLowRateHistory;

        double output = 0.0;
        for (std::size_t phase = 0U; phase < factor; ++phase) {
            const double upsampled = interpolate(phase, factor);
            const double processed = processor(upsampled);
            output_history_[output_write_] = processed;
            output_write_ = (output_write_ + 1U) % kMaxTaps;
            if (phase == 0U) output = decimate(factor);
        }
        return output;
    }

private:
    static constexpr std::size_t kLowRateHistory = kMaxTaps / 2U + 2U;

    [[nodiscard]] static constexpr std::size_t tap_count(std::size_t factor) noexcept {
        switch (factor) {
        case 2U: return 127U;
        case 4U: return 191U;
        case 8U: return 255U;
        case 16U: return 511U;
        default: return 1U;
        }
    }

    void design_filter() noexcept {
        const std::size_t factor = factor_value();
        const std::size_t taps = tap_count(factor);
        coefficients_.fill(0.0);
        if (factor == 1U) {
            coefficients_[0] = 1.0;
            tap_count_ = 1U;
            return;
        }
        tap_count_ = taps;
        const double center = static_cast<double>(taps - 1U) * 0.5;
        // Leave a controlled transition band below the original Nyquist so
        // nonlinear images are strongly rejected without unnecessarily
        // attenuating the audible upper band.
        const double cutoff = 0.45 / static_cast<double>(factor);
        for (std::size_t i = 0U; i < taps; ++i) {
            const double n = static_cast<double>(i) - center;
            const double sinc = std::abs(n) < 1.0e-15
                ? 2.0 * cutoff
                : std::sin(2.0 * std::numbers::pi * cutoff * n) / (std::numbers::pi * n);
            const double x = static_cast<double>(i) / static_cast<double>(taps - 1U);
            const double window = 0.35875 - 0.48829 * std::cos(2.0 * std::numbers::pi * x)
                                + 0.14128 * std::cos(4.0 * std::numbers::pi * x)
                                - 0.01168 * std::cos(6.0 * std::numbers::pi * x);
            coefficients_[i] = sinc * window * static_cast<double>(factor);
        }
    }

    [[nodiscard]] double interpolate(std::size_t phase, std::size_t factor) const noexcept {
        double sum = 0.0;
        const std::size_t taps = tap_count_;
        const std::size_t center = (taps - 1U) / 2U;
        for (std::size_t k = 0U; k < kLowRateHistory; ++k) {
            const std::size_t tap = k * factor + phase;
            if (tap >= taps) break;
            const std::size_t idx = (input_write_ + kLowRateHistory - 1U - k) % kLowRateHistory;
            sum += input_history_[idx] * coefficients_[tap];
        }
        (void)center;
        return sum;
    }

    [[nodiscard]] double decimate(std::size_t /*factor*/) const noexcept {
        // The decimator must convolve the complete high-rate stream. Sampling
        // only one phase here would let nonlinear images alias straight back
        // into the low-rate band.
        double sum = 0.0;
        const std::size_t taps = tap_count_;
        const std::size_t latest = (output_write_ + kMaxTaps - 1U) % kMaxTaps;
        for (std::size_t k = 0U; k < taps; ++k) {
            const std::size_t idx = (latest + kMaxTaps - k) % kMaxTaps;
            sum += output_history_[idx] * (coefficients_[k] / static_cast<double>(factor_value()));
        }
        return sum;
    }

    Factor factor_;
    std::array<double, kMaxTaps> coefficients_{};
    std::array<double, kLowRateHistory> input_history_{};
    std::array<double, kMaxTaps> output_history_{};
    std::size_t tap_count_ = 1U;
    std::size_t input_write_ = 0U;
    std::size_t output_write_ = 0U;
};

} // namespace studiohub::dsp::oversampling
