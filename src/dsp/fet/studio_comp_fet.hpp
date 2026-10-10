#pragma once

#include <cstddef>

namespace studiohub::dsp {

struct CompressorParams {
    double threshold_db = -18.0;
    double ratio = 4.0;
    double attack_ms = 0.5;
    double release_ms = 80.0;
    double input_gain_db = 0.0;
    double output_gain_db = 0.0;
    bool bypass = false;
    double knee_db = 0.0;
};

// Feed-forward, peak-detected stereo-linked compressor. Controls glide over
// 10 ms; the detector and wet/dry transition are sample-smoothed. No memory
// allocation or locks occur in process().
class StudioCompFET {
public:
    explicit StudioCompFET(double sample_rate = 48000.0) noexcept;

    void prepare(double sample_rate) noexcept;
    void reset() noexcept;
    void set_params(const CompressorParams& params) noexcept;
    [[nodiscard]] const CompressorParams& target_params() const noexcept;

    void process(float* const* channels, std::size_t num_channels,
                 std::size_t num_samples) noexcept;

    [[nodiscard]] double last_gain_reduction_db() const noexcept;

private:
    struct Values {
        double threshold_db;
        double ratio;
        double attack_ms;
        double release_ms;
        double input_gain_db;
        double output_gain_db;
        double knee_db;
        double wet;
    };

    double sample_rate_;
    CompressorParams target_;
    Values current_;
    Values target_values_;
    std::size_t parameter_ramp_remaining_;
    double envelope_;
    double gain_reduction_db_;
    double bypass_mix_;

    static CompressorParams sanitize(const CompressorParams& params) noexcept;
    void advance_parameters() noexcept;
};

} // namespace studiohub::dsp
