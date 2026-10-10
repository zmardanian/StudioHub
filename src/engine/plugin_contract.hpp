#pragma once

#include "../dsp/core/audio_buffer.hpp"

namespace studiohub::engine {

class PluginContract {
public:
    virtual ~PluginContract() = default;

    virtual void prepare(double sample_rate,
                         std::size_t max_block_size,
                         std::size_t channels) = 0;

    virtual void reset() noexcept = 0;

    virtual void process(dsp::AudioBuffer buffer) noexcept = 0;
};

} // namespace studiohub::engine
