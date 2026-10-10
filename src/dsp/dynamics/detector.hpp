#pragma once

#include <algorithm>
#include <cmath>

namespace studiohub::dsp::dynamics {

enum class DetectorMode { Peak, RMS };

class Detector {
public:
    explicit Detector(double sample_rate = 48000.0) noexcept : sample_rate_(sanitize_rate(sample_rate)) { update_coefficients(); }

    void prepare(double sample_rate) noexcept {
        if (std::isfinite(sample_rate) && sample_rate > 0.0) sample_rate_ = sample_rate;
        update_coefficients();
        reset();
    }

    void reset() noexcept { envelope_ = 0.0; }

    void set_mode(DetectorMode mode) noexcept { mode_ = mode; reset(); }

    void set_times(double attack_seconds, double release_seconds) noexcept {
        attack_seconds_ = sanitize_time(attack_seconds);
        release_seconds_ = sanitize_time(release_seconds);
        update_coefficients();
    }

    [[nodiscard]] double process(double sample) noexcept {
        const double x = std::isfinite(sample) ? std::abs(sample) : 0.0;
        const double target = mode_ == DetectorMode::Peak ? x : x * x;
        const double coefficient = target > envelope_ ? attack_coefficient_ : release_coefficient_;
        envelope_ += (target - envelope_) * (1.0 - coefficient);
        return mode_ == DetectorMode::Peak ? envelope_ : std::sqrt(std::max(0.0, envelope_));
    }

    [[nodiscard]] double envelope() const noexcept { return mode_ == DetectorMode::Peak ? envelope_ : std::sqrt(std::max(0.0, envelope_)); }

private:
    [[nodiscard]] static double sanitize_rate(double rate) noexcept { return std::isfinite(rate) && rate > 0.0 ? rate : 48000.0; }
    [[nodiscard]] static double sanitize_time(double seconds) noexcept { return std::isfinite(seconds) && seconds > 0.0 ? seconds : 0.0; }

    void update_coefficients() noexcept {
        attack_coefficient_ = attack_seconds_ == 0.0 ? 0.0 : std::exp(-1.0 / (attack_seconds_ * sample_rate_));
        release_coefficient_ = release_seconds_ == 0.0 ? 0.0 : std::exp(-1.0 / (release_seconds_ * sample_rate_));
    }

    double sample_rate_ = 48000.0;
    double attack_seconds_ = 0.001;
    double release_seconds_ = 0.100;
    double attack_coefficient_ = 0.0;
    double release_coefficient_ = 0.0;
    double envelope_ = 0.0;
    DetectorMode mode_ = DetectorMode::Peak;
};

} // namespace studiohub::dsp::dynamics
