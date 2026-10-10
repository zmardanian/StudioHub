#include <gtest/gtest.h>

#include "analysis/signal_generator.hpp"

#include <cmath>
#include <limits>
#include <numbers>

using studiohub::analysis::SineGenerator;

TEST(SineGenerator, DefaultConfiguration)
{
    const SineGenerator generator(48000.0);

    EXPECT_DOUBLE_EQ(generator.frequency(), 1000.0);
    EXPECT_DOUBLE_EQ(generator.amplitude(), 1.0);
    EXPECT_DOUBLE_EQ(generator.phase(), 0.0);
}

TEST(SineGenerator, GeneratesExpectedFirstSamples)
{
    SineGenerator generator(48000.0);

    generator.set_frequency(1000.0);
    generator.set_amplitude(1.0);
    generator.reset(0.0);

    EXPECT_NEAR(generator.next_sample(), 0.0, 1.0e-15);

    EXPECT_NEAR(
        generator.next_sample(),
        std::sin(2.0 * std::numbers::pi / 48.0),
        1.0e-15
    );
}

TEST(SineGenerator, AmplitudeScalesOutput)
{
    SineGenerator generator(48000.0);

    generator.set_frequency(1000.0);
    generator.set_amplitude(0.25);
    generator.reset(0.0);

    (void)generator.next_sample();

    EXPECT_NEAR(
        generator.next_sample(),
        0.25 * std::sin(2.0 * std::numbers::pi / 48.0),
        1.0e-15
    );
}

TEST(SineGenerator, FrequencyAndAmplitudeAreStored)
{
    SineGenerator generator(48000.0);

    generator.set_frequency(1234.5);
    generator.set_amplitude(0.37);

    EXPECT_DOUBLE_EQ(generator.frequency(), 1234.5);
    EXPECT_DOUBLE_EQ(generator.amplitude(), 0.37);
}

TEST(SineGenerator, PhaseWrapsIntoValidRange)
{
    SineGenerator generator(48000.0);

    generator.set_frequency(48000.0);
    generator.reset(0.0);

    for (int i = 0; i < 1000; ++i) {
        const double value = generator.next_sample();

        EXPECT_TRUE(std::isfinite(value));
        EXPECT_GE(generator.phase(), 0.0);
        EXPECT_LT(generator.phase(), 2.0 * std::numbers::pi);
    }
}

TEST(SineGenerator, NonFiniteFrequencyIsSafelyHandled)
{
    SineGenerator generator(48000.0);

    generator.set_frequency(
        std::numeric_limits<double>::quiet_NaN()
    );

    EXPECT_DOUBLE_EQ(generator.frequency(), 0.0);
    EXPECT_TRUE(std::isfinite(generator.next_sample()));
}

TEST(SineGenerator, NonFiniteAmplitudeIsSafelyHandled)
{
    SineGenerator generator(48000.0);

    generator.set_amplitude(
        std::numeric_limits<double>::infinity()
    );

    EXPECT_DOUBLE_EQ(generator.amplitude(), 0.0);
    EXPECT_DOUBLE_EQ(generator.next_sample(), 0.0);
}

TEST(SineGenerator, LongRunRemainsFinite)
{
    SineGenerator generator(48000.0);

    generator.set_frequency(997.0);
    generator.set_amplitude(0.5);

    for (int i = 0; i < 1000000; ++i) {
        const double sample = generator.next_sample();

        ASSERT_TRUE(std::isfinite(sample));
        ASSERT_TRUE(std::isfinite(generator.phase()));
    }
}
