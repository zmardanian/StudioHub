#include <gtest/gtest.h>

#include "analysis/sweep_generator.hpp"

#include <cmath>

using studiohub::analysis::LogSweepGenerator;

TEST(LogSweepGenerator, DefaultConfigurationIsValid)
{
    const LogSweepGenerator generator(48000.0);

    EXPECT_DOUBLE_EQ(generator.sample_rate(), 48000.0);
    EXPECT_FALSE(generator.finished());
    EXPECT_DOUBLE_EQ(generator.progress(), 0.0);
}

TEST(LogSweepGenerator, ProducesFiniteSamples)
{
    LogSweepGenerator generator(48000.0);

    generator.configure(
        20.0,
        20000.0,
        1.0,
        0.5
    );

    for (int i = 0; i < 10000; ++i) {
        const double sample =
            generator.next_sample();

        ASSERT_TRUE(std::isfinite(sample));
    }
}

TEST(LogSweepGenerator, ProgressIsMonotonic)
{
    LogSweepGenerator generator(48000.0);

    generator.configure(
        20.0,
        20000.0,
        1.0,
        1.0
    );

    double previous = generator.progress();

    for (int i = 0; i < 1000; ++i) {
        (void)generator.next_sample();

        const double current =
            generator.progress();

        EXPECT_GE(current, previous);
        previous = current;
    }
}

TEST(LogSweepGenerator, FrequencyIncreases)
{
    LogSweepGenerator generator(48000.0);

    generator.configure(
        20.0,
        20000.0,
        1.0,
        1.0
    );

    const double initial =
        generator.current_frequency();

    for (int i = 0; i < 10000; ++i) {
        (void)generator.next_sample();
    }

    const double later =
        generator.current_frequency();

    EXPECT_GT(later, initial);
}

TEST(LogSweepGenerator, ReachesEnd)
{
    LogSweepGenerator generator(48000.0);

    generator.configure(
        20.0,
        20000.0,
        0.01,
        1.0
    );

    int samples = 0;

    while (!generator.finished()) {
        (void)generator.next_sample();
        ++samples;

        ASSERT_LT(samples, 10000);
    }

    EXPECT_EQ(samples, 480);
    EXPECT_DOUBLE_EQ(generator.progress(), 1.0);
}

TEST(LogSweepGenerator, ResetRestartsSweep)
{
    LogSweepGenerator generator(48000.0);

    generator.configure(
        20.0,
        20000.0,
        1.0,
        1.0
    );

    (void)generator.next_sample();

    generator.reset();

    EXPECT_DOUBLE_EQ(generator.progress(), 0.0);
    EXPECT_NEAR(
        generator.current_frequency(),
        20.0,
        1.0e-12
    );
}
