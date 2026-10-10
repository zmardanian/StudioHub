#include "dsp/core/dsp_core.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace studiohub::dsp {

DspCore::DspCore(const double sample_rate)
    : sample_rate_(std::isfinite(sample_rate) && sample_rate >= kMinSampleRate && sample_rate <= kMaxSampleRate
                       ? sample_rate
                       : 48000.0)
{
}

double DspCore::sample_rate() const noexcept { return sample_rate_; }

void DspCore::clear() noexcept {}

void DspCore::process(std::span<float> samples) noexcept
{
    // Identity baseline: preserve finite samples and make non-finite input fail closed.
    for (float& sample : samples) {
        if (!std::isfinite(sample)) sample = 0.0F;
    }
}

} // namespace studiohub::dsp
