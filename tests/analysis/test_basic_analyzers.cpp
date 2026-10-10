#include <gtest/gtest.h>
#include "analysis/analyzers.hpp"

TEST(BasicAnalyzer, MeasuresPeakRmsAndDc) {
    studiohub::analysis::BasicAnalyzer analyzer;
    analyzer.push(1.0);
    analyzer.push(-1.0);
    analyzer.push(0.0);

    const auto result = analyzer.result();

    EXPECT_DOUBLE_EQ(result.peak, 1.0);
    EXPECT_NEAR(result.rms, std::sqrt(2.0 / 3.0), 1.0e-12);
    EXPECT_DOUBLE_EQ(result.dc, 0.0);
}
