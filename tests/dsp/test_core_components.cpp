#include <gtest/gtest.h>

#include "dsp/core/audio_buffer.hpp"
#include "dsp/dynamics/detector.hpp"
#include "dsp/oversampling/oversampling.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using studiohub::dsp::AudioBuffer;
using studiohub::dsp::dynamics::Detector;
using studiohub::dsp::dynamics::DetectorMode;
using studiohub::dsp::oversampling::Factor;
using studiohub::dsp::oversampling::Oversampler;

TEST(AudioBuffer, InvalidAccessIsEmpty)
{
    AudioBuffer empty;
    EXPECT_FALSE(empty.channel(0).data());
    EXPECT_EQ(empty.channel_count(), 0U);

    float* channels[] = {nullptr};
    AudioBuffer invalid(channels, 1U, 64U);
    EXPECT_TRUE(invalid.channel(0).empty());
    EXPECT_TRUE(invalid.channel(1).empty());
}

TEST(AudioBuffer, ValidChannelIsExposed)
{
    std::array<float, 4> data{1.0F, 2.0F, 3.0F, 4.0F};
    float* channels[] = {data.data()};
    AudioBuffer buffer(channels, 1U, data.size());
    EXPECT_TRUE(buffer.valid());
    EXPECT_EQ(buffer.channel(0).size(), data.size());
    EXPECT_FLOAT_EQ(buffer.channel(0)[2], 3.0F);
}

TEST(Detector, PeakAttackAndReleaseAreMonotonic)
{
    Detector detector(48000.0);
    detector.set_mode(DetectorMode::Peak);
    detector.set_times(0.001, 0.050);
    double previous = detector.process(0.0);
    for (int i = 0; i < 100; ++i) {
        const double current = detector.process(1.0);
        EXPECT_GE(current, previous);
        previous = current;
    }
    previous = detector.process(0.0);
    for (int i = 0; i < 100; ++i) {
        const double current = detector.process(0.0);
        EXPECT_LE(current, previous);
        previous = current;
    }
}

TEST(Detector, RMSModeReturnsRmsDomain)
{
    Detector detector(48000.0);
    detector.set_mode(DetectorMode::RMS);
    detector.set_times(0.0, 0.0);
    EXPECT_NEAR(detector.process(0.5), 0.5, 1.0e-12);
    EXPECT_NEAR(detector.process(-0.25), 0.25, 1.0e-12);
}

TEST(Detector, NonFiniteInputFailsClosed)
{
    Detector detector;
    detector.set_times(0.0, 0.0);
    EXPECT_DOUBLE_EQ(detector.process(std::numeric_limits<double>::quiet_NaN()), 0.0);
    EXPECT_DOUBLE_EQ(detector.process(std::numeric_limits<double>::infinity()), 0.0);
}

TEST(Oversampler, FactorOneProcessesExactlyOnce)
{
    Oversampler os(Factor::x1);
    int calls = 0;
    const double out = os.process(0.25, [&](double x) noexcept { ++calls; return x * 2.0; });
    EXPECT_EQ(calls, 1);
    EXPECT_DOUBLE_EQ(out, 0.5);
}

TEST(Oversampler, HigherFactorsInvokeProcessorAtHighRate)
{
    for (const auto factor : {Factor::x2, Factor::x4, Factor::x8, Factor::x16}) {
        Oversampler os(factor);
        os.reset();
        std::size_t calls = 0U;
        double last = 0.0;
        for (int i = 0; i < 256; ++i) {
            last = os.process(std::sin(0.01 * i), [&](double x) noexcept { ++calls; return x; });
            ASSERT_TRUE(std::isfinite(last));
        }
        EXPECT_EQ(calls, 256U * static_cast<std::size_t>(factor));
    }
}

TEST(Oversampler, ConstantSignalSettlesNearUnity)
{
    Oversampler os(Factor::x4);
    double out = 0.0;
    for (int i = 0; i < 2000; ++i) out = os.process(0.25, [](double x) noexcept { return x; });
    EXPECT_NEAR(out, 0.25, 1.0e-3);
}


TEST(Oversampler, ChangingFactorRedesignsFilter)
{
    Oversampler os(Factor::x2);
    for (int i = 0; i < 500; ++i) (void)os.process(0.25, [](double x) noexcept { return x; });
    os.set_factor(Factor::x8);
    double out = 0.0;
    for (int i = 0; i < 2000; ++i) out = os.process(0.25, [](double x) noexcept { return x; });
    EXPECT_NEAR(out, 0.25, 2.0e-3);
}

TEST(Detector, PrepareRecomputesTimeConstants)
{
    Detector detector(48000.0);
    detector.set_times(0.010, 0.100);
    detector.prepare(96000.0);
    detector.set_times(0.010, 0.100);
    const double a = detector.process(1.0);
    detector.reset();
    detector.prepare(48000.0);
    detector.set_times(0.010, 0.100);
    const double b = detector.process(1.0);
    EXPECT_LT(a, b);
}


TEST(Oversampler, NonlinearAliasIsSuppressed) {
    constexpr double fs = 48000.0;
    constexpr std::size_t n = 65536U;
    studiohub::dsp::oversampling::Oversampler oversampler(studiohub::dsp::oversampling::Factor::x16);
    std::vector<double> output; output.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double x = 0.5 * std::sin(2.0 * std::numbers::pi * 15000.0 * static_cast<double>(i) / fs);
        output.push_back(oversampler.process(x, [](double v) noexcept { return v * v; }));
    }
    double real = 0.0;
    double imag = 0.0;
    const double alias_hz = 18000.0;
    for (std::size_t i = 4096U; i < output.size(); ++i) {
        const double phase = 2.0 * std::numbers::pi * alias_hz * static_cast<double>(i - 4096U) / fs;
        real += output[i] * std::cos(phase);
        imag -= output[i] * std::sin(phase);
    }
    const double magnitude = 2.0 * std::hypot(real, imag) / static_cast<double>(output.size() - 4096U);
    EXPECT_LT(magnitude, 3.0e-5);
}
