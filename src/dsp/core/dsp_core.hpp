#pragma once

#include <cstddef>
#include <span>

namespace studiohub::dsp {

class DspCore {
public:
    static constexpr double kMinSampleRate = 8000.0;
    static constexpr double kMaxSampleRate = 768000.0;

    explicit DspCore(double sample_rate);

    [[nodiscard]] double sample_rate() const noexcept;

    void clear() noexcept;

    void process(std::span<float> samples) noexcept;

private:
    double sample_rate_;
};

} // namespace studiohub::dsp
