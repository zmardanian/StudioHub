#include <gtest/gtest.h>

#include "dsp/core/parameter_smoother.hpp"

#include <cmath>
#include <limits>

using studiohub::dsp::LinearParameterSmoother;

TEST(ParameterSmoother, ResetSetsCurrentAndTarget)
{
    LinearParameterSmoother smoother(48000.0);

    smoother.reset(0.75);

    EXPECT_DOUBLE_EQ(smoother.current_value(), 0.75);
    EXPECT_DOUBLE_EQ(smoother.target_value(), 0.75);
    EXPECT_FALSE(smoother.is_smoothing());
}

TEST(ParameterSmoother, ReachesTargetAfterRequestedTime)
{
    LinearParameterSmoother smoother(1000.0);

    smoother.reset(0.0);
    smoother.set_target(1.0, 0.010);

    EXPECT_TRUE(smoother.is_smoothing());

    for (int i = 0; i < 10; ++i) {
        (void)smoother.get_next_value();
    }

    EXPECT_DOUBLE_EQ(smoother.current_value(), 1.0);
    EXPECT_DOUBLE_EQ(smoother.target_value(), 1.0);
    EXPECT_FALSE(smoother.is_smoothing());
}

TEST(ParameterSmoother, RampIsMonotonicUpward)
{
    LinearParameterSmoother smoother(1000.0);

    smoother.reset(0.0);
    smoother.set_target(1.0, 0.010);

    double previous = smoother.current_value();

    for (int i = 0; i < 10; ++i) {
        const double value = smoother.get_next_value();

        EXPECT_GE(value, previous);
        EXPECT_LE(value, 1.0);

        previous = value;
    }
}

TEST(ParameterSmoother, RampIsMonotonicDownward)
{
    LinearParameterSmoother smoother(1000.0);

    smoother.reset(1.0);
    smoother.set_target(0.0, 0.010);

    double previous = smoother.current_value();

    for (int i = 0; i < 10; ++i) {
        const double value = smoother.get_next_value();

        EXPECT_LE(value, previous);
        EXPECT_GE(value, 0.0);

        previous = value;
    }

    EXPECT_DOUBLE_EQ(smoother.current_value(), 0.0);
}

TEST(ParameterSmoother, ZeroTimeChangesImmediately)
{
    LinearParameterSmoother smoother(48000.0);

    smoother.reset(0.0);
    smoother.set_target(1.0, 0.0);

    EXPECT_DOUBLE_EQ(smoother.current_value(), 1.0);
    EXPECT_DOUBLE_EQ(smoother.get_next_value(), 1.0);
    EXPECT_FALSE(smoother.is_smoothing());
}

TEST(ParameterSmoother, NegativeTimeChangesImmediately)
{
    LinearParameterSmoother smoother(48000.0);

    smoother.reset(0.0);
    smoother.set_target(1.0, -0.1);

    EXPECT_DOUBLE_EQ(smoother.current_value(), 1.0);
    EXPECT_FALSE(smoother.is_smoothing());
}

TEST(ParameterSmoother, RetargetingStartsFromCurrentValue)
{
    LinearParameterSmoother smoother(1000.0);

    smoother.reset(0.0);
    smoother.set_target(1.0, 0.010);

    const double first = smoother.get_next_value();

    smoother.set_target(0.0, 0.010);

    const double second = smoother.get_next_value();

    EXPECT_LT(second, first);
    EXPECT_GT(second, 0.0);
}

TEST(ParameterSmoother, PrepareChangesSampleRate)
{
    LinearParameterSmoother smoother(1000.0);

    smoother.reset(0.0);
    smoother.prepare(2000.0);
    smoother.set_target(1.0, 0.010);

    for (int i = 0; i < 20; ++i) {
        (void)smoother.get_next_value();
    }

    EXPECT_DOUBLE_EQ(smoother.current_value(), 1.0);
}

TEST(ParameterSmoother, NonFiniteTargetDoesNotLeaveActiveRamp)
{
    LinearParameterSmoother smoother(48000.0);

    smoother.reset(0.0);
    smoother.set_target(
        std::numeric_limits<double>::quiet_NaN(),
        0.010
    );

    EXPECT_FALSE(smoother.is_smoothing());
    EXPECT_DOUBLE_EQ(smoother.current_value(), 0.0);
    EXPECT_DOUBLE_EQ(smoother.target_value(), 0.0);
}
