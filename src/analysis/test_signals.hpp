#pragma once

#include <cstdint>
#include <cstddef>

namespace studiohub::analysis {

class ImpulseGenerator {
public:
    explicit ImpulseGenerator(
        double amplitude = 1.0) noexcept;

    void reset() noexcept;

    [[nodiscard]]
    double next_sample() noexcept;

    [[nodiscard]]
    bool emitted() const noexcept;

private:
    double amplitude_;
    bool emitted_;
};

class WhiteNoiseGenerator {
public:
    explicit WhiteNoiseGenerator(
        std::uint64_t seed = 0x9E3779B97F4A7C15ULL) noexcept;

    void reset(
        std::uint64_t seed) noexcept;

    void set_amplitude(
        double amplitude) noexcept;

    [[nodiscard]]
    double next_sample() noexcept;

    [[nodiscard]]
    double amplitude() const noexcept;

private:
    std::uint64_t state_;
    double amplitude_;

    [[nodiscard]]
    std::uint64_t next_uint64() noexcept;
};

} // namespace studiohub::analysis
