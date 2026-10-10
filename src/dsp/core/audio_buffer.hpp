#pragma once

#include <cstddef>
#include <span>

namespace studiohub::dsp {

class AudioBuffer {
public:
    AudioBuffer() noexcept = default;
    AudioBuffer(float* const* channels, std::size_t channel_count, std::size_t sample_count) noexcept
        : channels_(channels), channel_count_(channel_count), sample_count_(sample_count) {}

    [[nodiscard]] std::size_t channel_count() const noexcept { return channel_count_; }
    [[nodiscard]] std::size_t sample_count() const noexcept { return sample_count_; }
    [[nodiscard]] bool valid() const noexcept { return channel_count_ == 0U || channels_ != nullptr; }

    [[nodiscard]] std::span<float> channel(std::size_t index) noexcept {
        if (!valid() || index >= channel_count_ || channels_[index] == nullptr) return {};
        return {channels_[index], sample_count_};
    }

    [[nodiscard]] std::span<const float> channel(std::size_t index) const noexcept {
        if (!valid() || index >= channel_count_ || channels_[index] == nullptr) return {};
        return {channels_[index], sample_count_};
    }

private:
    float* const* channels_ = nullptr;
    std::size_t channel_count_ = 0;
    std::size_t sample_count_ = 0;
};

} // namespace studiohub::dsp
