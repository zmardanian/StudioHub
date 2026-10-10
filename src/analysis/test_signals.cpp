#include "analysis/test_signals.hpp"

#include <cmath>
#include <limits>

namespace studiohub::analysis {

ImpulseGenerator::ImpulseGenerator(
    const double amplitude) noexcept
    : amplitude_(
        std::isfinite(amplitude)
            ? amplitude
            : 0.0),
      emitted_(false)
{
}

void ImpulseGenerator::reset() noexcept
{
    emitted_ = false;
}

double ImpulseGenerator::next_sample() noexcept
{
    if (emitted_) {
        return 0.0;
    }

    emitted_ = true;
    return amplitude_;
}

bool ImpulseGenerator::emitted() const noexcept
{
    return emitted_;
}

WhiteNoiseGenerator::WhiteNoiseGenerator(
    const std::uint64_t seed) noexcept
    : state_(seed != 0U ? seed : 0x9E3779B97F4A7C15ULL),
      amplitude_(1.0)
{
}

void WhiteNoiseGenerator::reset(
    const std::uint64_t seed) noexcept
{
    state_ =
        seed != 0U
            ? seed
            : 0x9E3779B97F4A7C15ULL;
}

void WhiteNoiseGenerator::set_amplitude(
    const double amplitude) noexcept
{
    amplitude_ =
        std::isfinite(amplitude)
            ? amplitude
            : 0.0;
}

double WhiteNoiseGenerator::next_sample() noexcept
{
    /*
     * xorshift64*:
     * deterministic, fast and adequate as a measurement stimulus
     * source. It is not intended as a cryptographic RNG.
     */
    const std::uint64_t random = next_uint64();

    const std::uint64_t mantissa =
        random >> 11U;

    constexpr double scale =
        1.0 / 9007199254740992.0;

    const double unit =
        static_cast<double>(mantissa) * scale;

    return amplitude_ * (2.0 * unit - 1.0);
}

double WhiteNoiseGenerator::amplitude() const noexcept
{
    return amplitude_;
}

std::uint64_t WhiteNoiseGenerator::next_uint64() noexcept
{
    std::uint64_t x = state_;

    x ^= x >> 12U;
    x ^= x << 25U;
    x ^= x >> 27U;

    state_ = x;

    return x * 2685821657736338717ULL;
}

} // namespace studiohub::analysis
