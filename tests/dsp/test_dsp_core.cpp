#include <gtest/gtest.h>

#include "dsp/core/dsp_core.hpp"

#include <array>
#include <cmath>

using studiohub::dsp::DspCore;

TEST(DspCore, StoresSampleRate)
{
    constexpr double sample_rate = 48000.0;

    const DspCore dsp(sample_rate);

    EXPECT_DOUBLE_EQ(dsp.sample_rate(), sample_rate);
}

TEST(DspCore, AcceptsStandardSampleRates)
{
    EXPECT_NO_FATAL_FAILURE(DspCore(44100.0));
    EXPECT_NO_FATAL_FAILURE(DspCore(48000.0));
    EXPECT_NO_FATAL_FAILURE(DspCore(88200.0));
    EXPECT_NO_FATAL_FAILURE(DspCore(96000.0));
    EXPECT_NO_FATAL_FAILURE(DspCore(192000.0));
}

TEST(DspCore, IdentityProcessDoesNotModifySamples)
{
    DspCore dsp(48000.0);

    std::array<float, 8> samples{
        -1.0F,
        -0.5F,
        -0.125F,
        0.0F,
        0.125F,
        0.5F,
        0.75F,
        1.0F
    };

    const auto original = samples;

    dsp.process(samples);

    EXPECT_EQ(samples, original);
}

TEST(DspCore, ClearIsSafe)
{
    DspCore dsp(48000.0);

    EXPECT_NO_FATAL_FAILURE(dsp.clear());
}

TEST(DspCore, SampleRateLimitsAreDefined)
{
    EXPECT_DOUBLE_EQ(DspCore::kMinSampleRate, 8000.0);
    EXPECT_DOUBLE_EQ(DspCore::kMaxSampleRate, 768000.0);
}


TEST(DspCore, InvalidSampleRateFallsBackSafely)
{
    const DspCore nan_dsp(std::numeric_limits<double>::quiet_NaN());
    const DspCore low_dsp(100.0);
    const DspCore high_dsp(1000000.0);
    EXPECT_DOUBLE_EQ(nan_dsp.sample_rate(), 48000.0);
    EXPECT_DOUBLE_EQ(low_dsp.sample_rate(), 48000.0);
    EXPECT_DOUBLE_EQ(high_dsp.sample_rate(), 48000.0);
}

TEST(DspCore, NonFiniteInputIsSanitized)
{
    DspCore dsp(48000.0);
    std::array<float, 4> samples{
        0.25F,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -0.5F
    };
    dsp.process(samples);
    EXPECT_FLOAT_EQ(samples[0], 0.25F);
    EXPECT_FLOAT_EQ(samples[1], 0.0F);
    EXPECT_FLOAT_EQ(samples[2], 0.0F);
    EXPECT_FLOAT_EQ(samples[3], -0.5F);
}
