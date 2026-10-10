#include <gtest/gtest.h>
#include "analysis/level_analyzer.hpp"
#include <cmath>
#include <limits>

using studiohub::analysis::LevelAnalyzer;

TEST(LevelAnalyzer, MeasuresPeakRmsAndDc) {
    const double samples[] = {1.0, -1.0, 0.0};
    LevelAnalyzer a;
    a.process(samples, 3U);
    const auto m = a.measurement();
    EXPECT_DOUBLE_EQ(m.peak, 1.0);
    EXPECT_NEAR(m.rms, std::sqrt(2.0 / 3.0), 1.0e-12);
    EXPECT_DOUBLE_EQ(m.dc, 0.0);
    EXPECT_DOUBLE_EQ(m.peak_dbfs(), 0.0);
}

TEST(LevelAnalyzer, IgnoresNonFiniteSamples) {
    const double samples[] = {1.0, std::numeric_limits<double>::quiet_NaN(), 0.5};
    LevelAnalyzer a;
    a.process(samples, 3U);
    const auto m = a.measurement();
    EXPECT_DOUBLE_EQ(m.peak, 1.0);
    EXPECT_TRUE(std::isfinite(m.rms));
}

TEST(LevelAnalyzer, NullInputIsSafe) {
    LevelAnalyzer a;
    a.process(static_cast<const double*>(nullptr), 10U);
    const auto m = a.measurement();
    EXPECT_DOUBLE_EQ(m.peak, 0.0);
    EXPECT_DOUBLE_EQ(m.rms, 0.0);
    EXPECT_DOUBLE_EQ(m.dc, 0.0);
}

TEST(LevelAnalyzer, ResetClearsState) {
    const double samples[] = {1.0, 1.0};
    LevelAnalyzer a;
    a.process(samples, 2U);
    a.reset();
    EXPECT_DOUBLE_EQ(a.measurement().peak, 0.0);
}
