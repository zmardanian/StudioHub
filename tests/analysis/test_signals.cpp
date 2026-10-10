#include <gtest/gtest.h>

#include "analysis/test_signals.hpp"

#include <cmath>

using studiohub::analysis::ImpulseGenerator;
using studiohub::analysis::WhiteNoiseGenerator;

TEST(ImpulseGenerator, EmitsExactlyOneImpulse)
{
    ImpulseGenerator generator(0.75);

    EXPECT_FALSE(generator.emitted());
    EXPECT_DOUBLE_EQ(generator.next_sample(), 0.75);
    EXPECT_TRUE(generator.emitted());

    for (int i = 0; i < 100; ++i) {
        EXPECT_DOUBLE_EQ(generator.next_sample(), 0.0);
    }
}

TEST(ImpulseGenerator, ResetProducesAnotherImpulse)
{
    ImpulseGenerator generator(1.0);

    (void)generator.next_sample();
    generator.reset();

    EXPECT_FALSE(generator.emitted());
    EXPECT_DOUBLE_EQ(generator.next_sample(), 1.0);
}

TEST(ImpulseGenerator, NonFiniteAmplitudeIsSafe)
{
    ImpulseGenerator generator(
        std::numeric_limits<double>::quiet_NaN()
    );

    EXPECT_DOUBLE_EQ(generator.next_sample(), 0.0);
    EXPECT_TRUE(std::isfinite(generator.next_sample()));
}

TEST(WhiteNoiseGenerator, ProducesFiniteSamples)
{
    WhiteNoiseGenerator generator(12345);

    for (int i = 0; i < 100000; ++i) {
        const double sample = generator.next_sample();

        ASSERT_TRUE(std::isfinite(sample));
        ASSERT_GE(sample, -1.0);
        ASSERT_LE(sample, 1.0);
    }
}

TEST(WhiteNoiseGenerator, SameSeedProducesSameSequence)
{
    WhiteNoiseGenerator first(123456789);
    WhiteNoiseGenerator second(123456789);

    for (int i = 0; i < 1000; ++i) {
        EXPECT_DOUBLE_EQ(
            first.next_sample(),
            second.next_sample()
        );
    }
}

TEST(WhiteNoiseGenerator, DifferentSeedsProduceDifferentSequence)
{
    WhiteNoiseGenerator first(1);
    WhiteNoiseGenerator second(2);

    bool different = false;

    for (int i = 0; i < 100; ++i) {
        if (first.next_sample() != second.next_sample()) {
            different = true;
            break;
        }
    }

    EXPECT_TRUE(different);
}

TEST(WhiteNoiseGenerator, AmplitudeScalesOutput)
{
    WhiteNoiseGenerator generator(123);

    generator.set_amplitude(0.25);

    for (int i = 0; i < 10000; ++i) {
        const double sample = generator.next_sample();

        ASSERT_GE(sample, -0.25);
        ASSERT_LE(sample, 0.25);
    }

    EXPECT_DOUBLE_EQ(generator.amplitude(), 0.25);
}

TEST(WhiteNoiseGenerator, ResetReproducesSequence)
{
    WhiteNoiseGenerator generator(987654321);

    const double first = generator.next_sample();
    const double second = generator.next_sample();

    generator.reset(987654321);

    EXPECT_DOUBLE_EQ(generator.next_sample(), first);
    EXPECT_DOUBLE_EQ(generator.next_sample(), second);
}
