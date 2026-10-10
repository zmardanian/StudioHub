#pragma once

#include <cmath>

namespace studiohub::dsp::math {

inline constexpr double kMinPositive = 1.0e-30;

[[nodiscard]]
inline double db_to_linear(const double db) noexcept
{
    return std::pow(10.0, db / 20.0);
}

[[nodiscard]]
inline double linear_to_db(const double linear) noexcept
{
    if (!(linear > kMinPositive)) {
        return -600.0;
    }

    return 20.0 * std::log10(linear);
}

[[nodiscard]]
inline float db_to_linear(const float db) noexcept
{
    return std::pow(10.0F, db / 20.0F);
}

[[nodiscard]]
inline float linear_to_db(const float linear) noexcept
{
    if (!(linear > static_cast<float>(kMinPositive))) {
        return -600.0F;
    }

    return 20.0F * std::log10(linear);
}

template <typename T>
[[nodiscard]]
constexpr T clamp(
    const T value,
    const T minimum,
    const T maximum) noexcept
{
    return value < minimum ? minimum
         : value > maximum ? maximum
                           : value;
}

template <typename T>
[[nodiscard]]
inline bool is_finite(const T value) noexcept
{
    return std::isfinite(value);
}

template <typename T>
[[nodiscard]]
inline T sanitize_finite(
    const T value,
    const T fallback = T{0}) noexcept
{
    return is_finite(value) ? value : fallback;
}

} // namespace studiohub::dsp::math
