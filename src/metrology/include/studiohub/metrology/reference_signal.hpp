#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace studiohub::metrology {

enum class ReferenceSignalType { Sine, Silence, Impulse, TwoTone };

struct ReferenceSignalSpec {
    ReferenceSignalType type{ReferenceSignalType::Sine};
    double sample_rate_hz{48000.0};
    double frequency_hz{997.0};
    double frequency2_hz{1009.0};
    double amplitude_dbfs{-20.0};
    double amplitude2_dbfs{-20.0};
    double duration_seconds{1.0};
    double phase_radians{0.0};
};

struct ReferenceSignal {
    std::vector<double> samples;
    std::string specification_id;
};

[[nodiscard]] bool valid(const ReferenceSignalSpec& spec) noexcept;
[[nodiscard]] ReferenceSignal generate(const ReferenceSignalSpec& spec);
[[nodiscard]] std::string stable_id(const ReferenceSignalSpec& spec);

} // namespace studiohub::metrology
