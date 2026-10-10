#pragma once

#include <cmath>

namespace studiohub::dsp {

[[nodiscard]] inline float sanitize_sample(float x) noexcept {
    return std::isfinite(x) ? x : 0.0F;
}

[[nodiscard]] inline double sanitize_sample(double x) noexcept {
    return std::isfinite(x) ? x : 0.0;
}

} // namespace studiohub::dsp
