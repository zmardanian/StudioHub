#include <gtest/gtest.h>

#include "dsp/core/dsp_math.hpp"

#include <cmath>
#include <limits>

using namespace studiohub::dsp::math;

TEST(DspMath, DbToLinearReferenceValues)
{
    EXPECT_NEAR(db_to_linear(0.0), 1.0, 1.0e-12);
    EXPECT_NEAR(db_to_linear(6.020599913279624), 2.0, 1.0e-12);
    EXPECT_NEAR(db_to_linear(-6.020599913279624), 0.5, 1.0e-12);
    EXPECT_NEAR(db_to_linear(-20.0), 0.1, 1.0e-12);
}

TEST(DspMath, LinearToDbReferenceValues)
{
    EXPECT_NEAR(linear_to_db(1.0), 0.0, 1.0e-12);
    EXPECT_NEAR(linear_to_db(2.0), 6.020599913279624, 1.0e-12);
    EXPECT_NEAR(linear_to_db(0.5), -6.020599913279624, 1.0e-12);
    EXPECT_NEAR(linear_to_db(0.1), -20.0, 1.0e-12);
}

TEST(DspMath, DbLinearRoundTrip)
{
    constexpr double values[] = {
        -60.0,
        -24.0,
        -12.0,
        -6.0,
        0.0,
        6.0,
        12.0,
        24.0
    };

    for (const double db : values) {
        EXPECT_NEAR(linear_to_db(db_to_linear(db)), db, 1.0e-12);
    }
}

TEST(DspMath, NonPositiveLinearValuesMapToFloor)
{
    EXPECT_DOUBLE_EQ(linear_to_db(0.0), -600.0);
    EXPECT_DOUBLE_EQ(linear_to_db(-1.0), -600.0);
    EXPECT_DOUBLE_EQ(
        linear_to_db(std::numeric_limits<double>::quiet_NaN()),
        -600.0
    );
}

TEST(DspMath, ClampWorks)
{
    EXPECT_EQ(clamp(5, 0, 10), 5);
    EXPECT_EQ(clamp(-5, 0, 10), 0);
    EXPECT_EQ(clamp(15, 0, 10), 10);
}

TEST(DspMath, FiniteDetection)
{
    EXPECT_TRUE(is_finite(1.0));
    EXPECT_TRUE(is_finite(0.0));
    EXPECT_TRUE(is_finite(-1.0));

    EXPECT_FALSE(is_finite(std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(is_finite(-std::numeric_limits<double>::infinity()));
    EXPECT_FALSE(is_finite(std::numeric_limits<double>::quiet_NaN()));
}

TEST(DspMath, FiniteSanitization)
{
    EXPECT_DOUBLE_EQ(sanitize_finite(3.5), 3.5);

    EXPECT_DOUBLE_EQ(
        sanitize_finite(
            std::numeric_limits<double>::infinity(),
            7.0),
        7.0
    );

    EXPECT_DOUBLE_EQ(
        sanitize_finite(
            std::numeric_limits<double>::quiet_NaN(),
            -2.0),
        -2.0
    );
}
