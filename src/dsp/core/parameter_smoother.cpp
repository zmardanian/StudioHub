#include "dsp/core/parameter_smoother.hpp"

#include <algorithm>
#include <cmath>

namespace studiohub::dsp {

LinearParameterSmoother::LinearParameterSmoother(
    const double sample_rate) noexcept
    : sample_rate_(sample_rate > 0.0 ? sample_rate : 48000.0),
      current_(0.0),
      target_(0.0),
      increment_(0.0),
      remaining_samples_(0)
{
}

void LinearParameterSmoother::prepare(
    const double sample_rate) noexcept
{
    if (sample_rate > 0.0 && std::isfinite(sample_rate)) {
        sample_rate_ = sample_rate;
    }
}

void LinearParameterSmoother::reset(
    const double value) noexcept
{
    current_ = value;
    target_ = value;
    increment_ = 0.0;
    remaining_samples_ = 0;
}

void LinearParameterSmoother::set_target(
    const double target,
    const double time_seconds) noexcept
{
    if (!std::isfinite(target)) {
        target_ = current_;
        increment_ = 0.0;
        remaining_samples_ = 0;
        return;
    }

    target_ = target;

    if (!std::isfinite(time_seconds) ||
        time_seconds <= 0.0 ||
        sample_rate_ <= 0.0) {
        current_ = target_;
        increment_ = 0.0;
        remaining_samples_ = 0;
        return;
    }

    const auto samples = static_cast<std::size_t>(
        std::ceil(time_seconds * sample_rate_)
    );

    if (samples == 0 || current_ == target_) {
        current_ = target_;
        increment_ = 0.0;
        remaining_samples_ = 0;
        return;
    }

    remaining_samples_ = samples;
    increment_ = (target_ - current_) /
                 static_cast<double>(remaining_samples_);
}

double LinearParameterSmoother::get_next_value() noexcept
{
    if (remaining_samples_ == 0) {
        current_ = target_;
        return current_;
    }

    current_ += increment_;
    --remaining_samples_;

    if (remaining_samples_ == 0) {
        current_ = target_;
    }

    return current_;
}

double LinearParameterSmoother::current_value() const noexcept
{
    return current_;
}

double LinearParameterSmoother::target_value() const noexcept
{
    return target_;
}

bool LinearParameterSmoother::is_smoothing() const noexcept
{
    return remaining_samples_ != 0;
}

} // namespace studiohub::dsp
