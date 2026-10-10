#include "dsp/fet/studio_comp_fet.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

namespace {
std::atomic<bool> track_allocations{false};
std::atomic<unsigned> process_allocations{0};
}

void* operator new(std::size_t size)
{
    if (track_allocations.load(std::memory_order_relaxed))
        process_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
void operator delete[](void* memory, std::size_t size) noexcept
{
    ::operator delete(memory, size);
}

namespace {
#define REQUIRE(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ \
                  << ": " #condition << '\n'; \
        return 1; \
    } \
} while (false)

constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

bool all_finite(const std::vector<float>& samples)
{
    for (const auto sample : samples)
        if (!std::isfinite(sample))
            return false;
    return true;
}

std::vector<float> sine(double amplitude, double frequency, std::size_t count)
{
    std::vector<float> result(count);
    for (std::size_t i = 0; i < count; ++i)
        result[i] = static_cast<float>(amplitude * std::sin(
            2.0 * kPi * frequency * static_cast<double>(i) / kSampleRate));
    return result;
}

double rms(const std::vector<float>& values, std::size_t begin)
{
    double sum = 0.0;
    for (std::size_t i = begin; i < values.size(); ++i) {
        const double value = values[i];
        sum += value * value;
    }
    return std::sqrt(sum / static_cast<double>(values.size() - begin));
}

double advance_constant(studiohub::dsp::StudioCompFET& compressor,
                        float value, std::size_t samples, std::size_t& block_index)
{
    constexpr std::array<std::size_t, 4> block_sizes{1, 31, 127, 256};
    std::array<float, 256> block{};
    while (samples > 0) {
        const auto count = std::min({samples,
            block_sizes[block_index % block_sizes.size()], block.size()});
        std::fill_n(block.begin(), count, value);
        float* channels[] = {block.data()};
        compressor.process(channels, 1, count);
        samples -= count;
        ++block_index;
    }
    return compressor.last_gain_reduction_db();
}
}

int main()
{
    constexpr std::size_t frames = 144000;
    constexpr std::size_t settle = 96000;

    // 1:1 ratio leaves a steady tone unchanged.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        auto p = compressor.target_params();
        p.ratio = 1.0;
        compressor.set_params(p);
        auto audio = sine(0.5, 997.0, frames);
        const auto before = rms(audio, settle);
        float* channels[] = {audio.data()};
        compressor.process(channels, 1, audio.size());
        REQUIRE(std::abs(20.0 * std::log10(rms(audio, settle) / before)) < 0.12);
        REQUIRE(compressor.last_gain_reduction_db() < 0.01);
    }

    // Peak detector static curve: -6 dBFS with -18 dB threshold, 4:1 gives
    // 9 dB gain reduction. A long settled signal measures the actual output.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        auto p = compressor.target_params();
        p.threshold_db = -18.0;
        p.ratio = 4.0;
        p.attack_ms = 0.05;
        p.release_ms = 80.0;
        compressor.set_params(p);
        std::vector<float> audio(frames, 0.50118724F);
        float* channels[] = {audio.data()};
        compressor.process(channels, 1, audio.size());
        const double measured_gr = 20.0 * std::log10(0.50118724 / rms(audio, settle));
        std::cout << "steady_transfer_gr_db=" << measured_gr << '\n';
        REQUIRE(std::abs(measured_gr - 9.0) < 0.25);
    }

    // A 12 dB soft knee follows the quadratic transition curve and reaches
    // the original 4:1 slope at the upper knee boundary.
    {
        for (const double input_level_db : {-30.0, -24.0, -21.0,
                                             -18.0, -15.0, -12.0}) {
            studiohub::dsp::StudioCompFET compressor(kSampleRate);
            auto p = compressor.target_params();
            p.threshold_db = -18.0;
            p.ratio = 4.0;
            p.attack_ms = 0.05;
            p.knee_db = 12.0;
            compressor.set_params(p);
            std::vector<float> audio(frames, static_cast<float>(
                std::pow(10.0, input_level_db / 20.0)));
            float* channels[] = {audio.data()};
            compressor.process(channels, 1, audio.size());

            const double over_db = input_level_db - p.threshold_db;
            const double position = over_db + 0.5 * p.knee_db;
            const double expected_gr = position <= 0.0 ? 0.0 :
                position >= p.knee_db ? over_db * 0.75 :
                0.75 * position * position / (2.0 * p.knee_db);
            REQUIRE(std::abs(compressor.last_gain_reduction_db() - expected_gr)
                    < 0.025);
        }
    }

    // Verify actual attack and release ballistics against measured baseline
    // bounds at common sample rates. Irregular block sizes ensure the result
    // does not depend on how a host splits the audio callback.
    {
        for (const double sample_rate : {44100.0, 48000.0, 96000.0}) {
            studiohub::dsp::StudioCompFET compressor(sample_rate);
            auto p = compressor.target_params();
            p.threshold_db = -18.0;
            p.ratio = 4.0;
            p.attack_ms = 0.5;
            p.release_ms = 80.0;
            compressor.set_params(p);
            std::size_t block_index = 0;
            const auto samples_for_ms = [sample_rate](double ms) {
                return static_cast<std::size_t>(std::llround(
                    sample_rate * ms / 1000.0));
            };

            advance_constant(compressor, 0.0F, samples_for_ms(20.0), block_index);
            std::size_t elapsed = 0;
            const auto attack_at = [&](double ms) {
                const auto target = samples_for_ms(ms);
                const auto gr = advance_constant(compressor, 0.50118724F,
                                                   target - elapsed, block_index);
                elapsed = target;
                return gr;
            };
            const double attack_05ms = attack_at(0.5);
            const double attack_1ms = attack_at(1.0);
            const double attack_5ms = attack_at(5.0);
            const double attack_100ms = attack_at(100.0);

            std::size_t release_elapsed = 0;
            const auto release_at = [&](double ms) {
                const auto target = samples_for_ms(ms);
                const auto gr = advance_constant(compressor, 0.0F,
                    target - release_elapsed, block_index);
                release_elapsed = target;
                return gr;
            };
            const double release_5ms = release_at(5.0);
            const double release_20ms = release_at(20.0);
            const double release_80ms = release_at(80.0);

            std::cout << "dynamics_sr=" << sample_rate
                      << " attack_gr_db_0.5_1_5_100ms=" << attack_05ms << ','
                      << attack_1ms << ',' << attack_5ms << ',' << attack_100ms
                      << " release_gr_db_5_20_80ms=" << release_5ms << ','
                      << release_20ms << ',' << release_80ms << '\n';
            REQUIRE(attack_05ms > 5.8 && attack_05ms < 6.4);
            REQUIRE(attack_1ms > 7.85 && attack_1ms < 8.3);
            REQUIRE(attack_5ms > 8.8 && attack_5ms < 9.1);
            REQUIRE(std::abs(attack_100ms - 9.0) < 0.05);
            REQUIRE(release_5ms > 8.4 && release_5ms < 8.8);
            REQUIRE(release_20ms > 7.15 && release_20ms < 7.6);
            REQUIRE(release_80ms > 2.35 && release_80ms < 2.62);
        }
    }

    // Stereo linking: a loud left signal attenuates a quieter right signal by
    // the same gain reduction.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        auto p = compressor.target_params();
        p.threshold_db = -18.0;
        p.ratio = 4.0;
        p.attack_ms = 0.05;
        compressor.set_params(p);
        std::vector<float> left(frames, 0.50118724F);
        std::vector<float> right(frames, 0.1F);
        const auto before = rms(right, settle);
        float* channels[] = {left.data(), right.data()};
        compressor.process(channels, 2, frames);
        const double gr_right = 20.0 * std::log10(before / rms(right, settle));
        REQUIRE(std::abs(gr_right - 9.0) < 0.25);
    }

    // Below threshold is transparent after settling.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        auto p = compressor.target_params();
        p.threshold_db = -18.0;
        p.ratio = 8.0;
        compressor.set_params(p);
        auto audio = sine(0.01, 440.0, frames);
        const auto before = rms(audio, settle);
        float* channels[] = {audio.data()};
        compressor.process(channels, 1, audio.size());
        REQUIRE(std::abs(20.0 * std::log10(rms(audio, settle) / before)) < 0.12);
    }

    // Input and output gain are measured together with a 1:1 ratio.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        auto p = compressor.target_params();
        p.ratio = 1.0;
        p.input_gain_db = 6.0;
        p.output_gain_db = -3.0;
        compressor.set_params(p);
        std::vector<float> audio(2400, 0.1F);
        float* channels[] = {audio.data()};
        compressor.process(channels, 1, audio.size());
        const double measured_db = 20.0 * std::log10(
            static_cast<double>(audio.back()) / 0.1);
        REQUIRE(std::abs(measured_db - 3.0) < 0.1);
    }

    // Parameter automation is slewed over a finite ramp, limiting adjacent
    // output discontinuity on a steady signal.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        std::array<float, 1024> audio{};
        audio.fill(0.1F);
        float* channels[] = {audio.data()};
        compressor.process(channels, 1, 512);
        auto p = compressor.target_params();
        p.output_gain_db = 24.0;
        compressor.set_params(p);
        channels[0] = audio.data() + 512;
        compressor.process(channels, 1, audio.size() - 512);
        float largest_step = 0.0F;
        for (std::size_t i = 512; i < audio.size(); ++i)
            largest_step = std::max(largest_step, std::abs(audio[i] - audio[i - 1]));
        std::cout << "automation_largest_adjacent_step=" << largest_step << '\n';
        REQUIRE(largest_step < 0.012F);
    }

    // Bypass crossfades to the dry path and is numerically transparent once
    // its control ramp settles.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        auto p = compressor.target_params();
        p.threshold_db = -60.0;
        p.input_gain_db = 12.0;
        p.output_gain_db = -6.0;
        compressor.set_params(p);
        std::vector<float> audio(24000, 0.1F);
        float* channels[] = {audio.data()};
        compressor.process(channels, 1, 12000);
        p.bypass = true;
        compressor.set_params(p);
        std::fill(audio.begin() + 12000, audio.end(), 0.1F);
        channels[0] = audio.data() + 12000;
        compressor.process(channels, 1, 12000);
        REQUIRE(std::abs(audio.back() - 0.1F) < 1.0e-5F);
    }

    // Parameter changes glide, invalid controls are clamped, and edge samples
    // cannot propagate NaN or infinity to the output.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        auto p = compressor.target_params();
        p.threshold_db = std::numeric_limits<double>::quiet_NaN();
        p.ratio = 1000.0;
        p.attack_ms = -1.0;
        p.release_ms = std::numeric_limits<double>::infinity();
        p.knee_db = 100.0;
        compressor.set_params(p);
        REQUIRE(compressor.target_params().threshold_db == -18.0);
        REQUIRE(compressor.target_params().ratio == 20.0);
        REQUIRE(compressor.target_params().attack_ms == 0.05);
        REQUIRE(compressor.target_params().release_ms == 80.0);
        REQUIRE(compressor.target_params().knee_db == 12.0);
        std::array<float, 5> audio{
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
            1.0e30F, -1.0e30F};
        float* channels[] = {audio.data()};
        compressor.process(channels, 1, audio.size());
        for (float sample : audio)
            REQUIRE(std::isfinite(sample));
    }

    // Processing benchmark: this is a measurement, not a machine-specific
    // pass threshold. One block spans exactly ten seconds of audio.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        std::vector<float> left(480000, 0.2F), right(480000, 0.15F);
        float* channels[] = {left.data(), right.data()};
        const auto start = std::chrono::steady_clock::now();
        compressor.process(channels, 2, left.size());
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << "ten_seconds_audio_processed_in_seconds=" << elapsed
                  << " realtime_factor=" << (10.0 / elapsed) << '\n';
        REQUIRE(all_finite(left));
        REQUIRE(all_finite(right));
    }

    // No C++ heap allocation occurs during a steady-state audio callback.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        std::array<float, 256> left{}, right{};
        left.fill(0.25F);
        right.fill(0.2F);
        float* channels[] = {left.data(), right.data()};
        process_allocations.store(0, std::memory_order_relaxed);
        track_allocations.store(true, std::memory_order_relaxed);
        compressor.process(channels, 2, left.size());
        track_allocations.store(false, std::memory_order_relaxed);
        REQUIRE(process_allocations.load(std::memory_order_relaxed) == 0);
    }

    // Silence remains finite with no false gain reduction.
    {
        studiohub::dsp::StudioCompFET compressor(kSampleRate);
        std::array<float, 512> silence{};
        float* channels[] = {silence.data()};
        compressor.process(channels, 1, silence.size());
        REQUIRE(compressor.last_gain_reduction_db() < 0.001);
        for (float value : silence)
            REQUIRE(std::isfinite(value) && value == 0.0F);
    }

    std::cout << "StudioComp FET DSP acceptance PASS\n";
}
