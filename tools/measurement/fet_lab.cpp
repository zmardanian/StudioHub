#include "dsp/fet/studio_comp_fet.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <string>
#include <stdexcept>
#include <vector>

namespace {
constexpr std::uint32_t kSampleRate = 48000;
constexpr std::uint16_t kChannels = 2;
constexpr std::size_t kFramesPerFixture = kSampleRate * 2;

void put_u16(std::ostream& out, std::uint16_t value)
{
    out.put(static_cast<char>(value & 0xffU));
    out.put(static_cast<char>((value >> 8U) & 0xffU));
}

void put_u32(std::ostream& out, std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.put(static_cast<char>((value >> shift) & 0xffU));
}

void write_wav(const std::filesystem::path& path,
               const std::vector<float>& left, const std::vector<float>& right)
{
    const auto data_bytes = static_cast<std::uint32_t>(
        (left.size() + right.size()) * sizeof(std::int16_t));
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4);
    put_u32(out, 36U + data_bytes);
    out.write("WAVEfmt ", 8);
    put_u32(out, 16);
    put_u16(out, 1);
    put_u16(out, kChannels);
    put_u32(out, kSampleRate);
    put_u32(out, kSampleRate * kChannels * sizeof(std::int16_t));
    put_u16(out, kChannels * sizeof(std::int16_t));
    put_u16(out, 16);
    out.write("data", 4);
    put_u32(out, data_bytes);
    for (std::size_t i = 0; i < left.size(); ++i) {
        for (const float sample : {left[i], right[i]}) {
            const auto clipped = std::clamp(static_cast<double>(sample), -1.0, 1.0);
            const auto pcm = static_cast<std::int16_t>(std::lrint(clipped * 32767.0));
            put_u16(out, static_cast<std::uint16_t>(pcm));
        }
    }
    if (!out)
        throw std::runtime_error("failed writing WAV: " + path.string());
}

double rms(const std::vector<float>& values, std::size_t first)
{
    double sum = 0.0;
    for (std::size_t i = first; i < values.size(); ++i) {
        const double sample = values[i];
        sum += sample * sample;
    }
    return std::sqrt(sum / static_cast<double>(values.size() - first));
}

std::vector<float> make_tone(double level_db, double frequency, std::size_t frames)
{
    const double amplitude = std::pow(10.0, level_db / 20.0);
    std::vector<float> result(frames);
    for (std::size_t i = 0; i < frames; ++i)
        result[i] = static_cast<float>(amplitude * std::sin(
            2.0 * std::numbers::pi * frequency * static_cast<double>(i) / kSampleRate));
    return result;
}

std::vector<float> make_listening_fixture(const std::string& kind, std::size_t frames)
{
    std::vector<float> result(frames, 0.0F);
    std::uint32_t noise_state = 0xA531E29BU;
    auto noise = [&noise_state]() {
        noise_state = noise_state * 1664525U + 1013904223U;
        return (static_cast<double>(noise_state) / 2147483648.0) - 1.0;
    };
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        const double beat = std::fmod(t, 0.5);
        if (kind == "vocal-like") {
            const double syllable = std::pow(std::max(0.0, std::sin(2.0 * std::numbers::pi * 2.1 * t)), 1.5);
            const double phase = 2.0 * std::numbers::pi * (145.0 * t + 3.0 * std::sin(2.0 * std::numbers::pi * 5.0 * t) / (2.0 * std::numbers::pi * 5.0));
            result[i] = static_cast<float>(0.24 * syllable * (std::sin(phase) + 0.32 * std::sin(2.0 * phase) + 0.17 * std::sin(3.0 * phase) + 0.10 * std::sin(2.0 * std::numbers::pi * 720.0 * t)));
        } else if (kind == "percussion") {
            const double kick_env = std::exp(-beat * 18.0);
            const double kick_phase = 2.0 * std::numbers::pi * (65.0 * beat - 28.0 * beat * beat);
            const double snare_offset = std::fmod(t + 0.25, 0.5);
            const double snare_env = std::exp(-snare_offset * 32.0);
            const double hat_offset = std::fmod(t, 0.25);
            const double hat_env = std::exp(-hat_offset * 90.0);
            const double kick = (beat < 0.18 ? 0.8 * kick_env * std::sin(kick_phase) : 0.0);
            const double snare = (snare_offset < 0.12 ? 0.24 * snare_env * (0.65 * noise() + 0.35 * std::sin(2.0 * std::numbers::pi * 190.0 * t)) : 0.0);
            const double hat = (hat_offset < 0.035 ? 0.08 * hat_env * noise() : 0.0);
            result[i] = static_cast<float>(kick + snare + hat);
        } else if (kind == "bass") {
            const double note = std::fmod(t, 1.0) < 0.5 ? 55.0 : 73.42;
            const double phase = 2.0 * std::numbers::pi * note * t;
            const double pluck = 0.78 + 0.22 * std::exp(-std::fmod(t, 0.5) * 4.0);
            result[i] = static_cast<float>(0.38 * pluck * (std::sin(phase) + 0.28 * std::sin(2.0 * phase) + 0.11 * std::sin(3.0 * phase)));
        } else if (kind == "full-mix") {
            const double vocal_env = std::pow(std::max(0.0, std::sin(2.0 * std::numbers::pi * 1.1 * t)), 1.5);
            const double vocal = 0.18 * vocal_env * (std::sin(2.0 * std::numbers::pi * 165.0 * t) + 0.22 * std::sin(2.0 * std::numbers::pi * 330.0 * t) + 0.1 * std::sin(2.0 * std::numbers::pi * 660.0 * t));
            const double bass = 0.23 * (std::sin(2.0 * std::numbers::pi * 55.0 * t) + 0.18 * std::sin(2.0 * std::numbers::pi * 110.0 * t));
            const double drums = (beat < 0.16 ? 0.42 * std::exp(-beat * 20.0) * std::sin(2.0 * std::numbers::pi * (62.0 * beat - 22.0 * beat * beat)) : 0.0) +
                (std::fmod(t + 0.25, 0.5) < 0.07 ? 0.12 * std::exp(-std::fmod(t + 0.25, 0.5) * 35.0) * noise() : 0.0);
            result[i] = static_cast<float>(vocal + bass + drums);
        }
    }
    return result;
}

std::vector<float> process_signal(const std::vector<float>& source, double knee_db)
{
    auto output = source;
    auto right = source;
    studiohub::dsp::StudioCompFET compressor(kSampleRate);
    auto params = compressor.target_params();
    params.threshold_db = -18.0;
    params.ratio = 4.0;
    params.attack_ms = 0.05;
    params.release_ms = 80.0;
    params.knee_db = knee_db;
    compressor.set_params(params);
    float* channels[] = {output.data(), right.data()};
    constexpr std::size_t block_size = 256;
    for (std::size_t offset = 0; offset < output.size(); offset += block_size) {
        compressor.process(channels, kChannels, std::min(block_size, output.size() - offset));
        channels[0] += std::min(block_size, output.size() - offset);
        channels[1] += std::min(block_size, output.size() - offset);
    }
    return output;
}

std::vector<float> process(double knee_db, double input_db)
{
    auto left = make_tone(input_db, 997.0, kFramesPerFixture);
    auto right = left;
    studiohub::dsp::StudioCompFET compressor(kSampleRate);
    auto params = compressor.target_params();
    params.threshold_db = -18.0;
    params.ratio = 4.0;
    params.attack_ms = 0.05;
    params.release_ms = 80.0;
    params.knee_db = knee_db;
    compressor.set_params(params);
    float* channels[] = {left.data(), right.data()};
    compressor.process(channels, kChannels, left.size());
    return left;
}
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 4) {
        std::cerr << "usage: StudioCompFETLab OUTPUT_DIRECTORY [SOURCE_REVISION] [PLUGIN_MODULE_SHA256]\n";
        return 2;
    }
    try {
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output);
        std::ofstream report(output / "measurements.json");
        report << std::setprecision(10)
               << "{\n  \"plugin\": \"StudioComp FET DSP\",\n"
               << "  \"source_revision\": \""
               << (argc >= 3 ? argv[2] : "unspecified") << "\",\n"
               << "  \"plugin_module_sha256\": \""
               << (argc >= 4 ? argv[3] : "unspecified") << "\",\n"
               << "  \"sample_rate_hz\": " << kSampleRate << ",\n"
               << "  \"frames_per_fixture\": " << kFramesPerFixture << ",\n"
               << "  \"process_call_frames\": " << kFramesPerFixture << ",\n"
               << "  \"input_frequency_hz\": 997,\n"
               << "  \"settings\": {\"threshold_db\": -18, \"ratio\": 4,"
                  " \"attack_ms\": 0.05, \"release_ms\": 80},\n"
               << "  \"transfer\": [\n";
        constexpr std::array<double, 9> levels{-36, -30, -24, -21, -18,
                                                -15, -12, -6, 0};
        for (std::size_t index = 0; index < levels.size(); ++index) {
            const double input_db = levels[index];
            auto dry_left = make_tone(input_db, 997.0, kFramesPerFixture);
            auto dry_right = dry_left;
            auto hard_left = process(0.0, input_db);
            auto knee_left = process(12.0, input_db);
            std::vector<float> hard_right = hard_left;
            std::vector<float> knee_right = knee_left;
            const std::string suffix = std::to_string(static_cast<int>(input_db)) + "dB";
            write_wav(output / ("dry-" + suffix + ".wav"), dry_left, dry_right);
            write_wav(output / ("hard-knee-" + suffix + ".wav"), hard_left, hard_right);
            write_wav(output / ("soft-knee-" + suffix + ".wav"), knee_left, knee_right);
            const auto settle = static_cast<std::size_t>(kSampleRate * 1.5);
            const double hard_gr = 20.0 * std::log10(
                std::pow(10.0, input_db / 20.0) /
                (std::sqrt(2.0) * rms(hard_left, settle)));
            const double knee_gr = 20.0 * std::log10(
                std::pow(10.0, input_db / 20.0) /
                (std::sqrt(2.0) * rms(knee_left, settle)));
            const double over = input_db + 18.0;
            const double expected_hard = over > 0.0 ? over * 0.75 : 0.0;
            const double pos = over + 6.0;
            const double expected_knee = pos <= 0.0 ? 0.0 : pos >= 12.0
                ? over * 0.75 : 0.75 * pos * pos / 24.0;
            report << "    {\"input_dbfs\": " << input_db
                   << ", \"hard_knee_output_gr_db\": " << hard_gr
                   << ", \"hard_knee_expected_gr_db\": " << expected_hard
                   << ", \"hard_knee_error_db\": " << hard_gr - expected_hard
                   << ", \"soft_knee_output_gr_db\": " << knee_gr
                   << ", \"soft_knee_expected_gr_db\": " << expected_knee
                   << ", \"soft_knee_error_db\": " << knee_gr - expected_knee
                   << "}" << (index + 1 == levels.size() ? "\n" : ",\n");
        }
        report << "  ],\n  \"listening\": {\"matching\": \"same input level;"
                   " no loudness compensation applied\", \"human_review_required\": true}\n}\n";
        if (!report)
            throw std::runtime_error("failed writing measurement report");
        std::cout << "wrote deterministic 48 kHz hard/soft knee sine renders and measurements to "
                  << output << '\n';
        const auto listening = output / "listening-fixtures";
        std::filesystem::create_directories(listening);
        constexpr std::array<const char*, 4> kinds{"vocal-like", "percussion", "bass", "full-mix"};
        constexpr std::size_t listening_frames = kSampleRate * 8;
        for (const auto* kind : kinds) {
            const auto dry = make_listening_fixture(kind, listening_frames);
            const auto hard = process_signal(dry, 0.0);
            const auto soft = process_signal(dry, 12.0);
            std::vector<float> dry_right = dry, hard_right = hard, soft_right = soft;
            write_wav(listening / (std::string(kind) + "-dry.wav"), dry, dry_right);
            write_wav(listening / (std::string(kind) + "-hard-knee.wav"), hard, hard_right);
            write_wav(listening / (std::string(kind) + "-soft-knee.wav"), soft, soft_right);
        }
        std::cout << "wrote synthetic listening fixtures (vocal-like, percussion, bass, full-mix); "
                     "human listening is still required\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
