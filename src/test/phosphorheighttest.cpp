#include <gtest/gtest.h>

#include "waveform/phosphor/phosphorheight.h"

namespace {

using namespace mixxx::phosphor;
using mixxx::waveformgains::Inputs;

constexpr float kLevels[3] = {0.8f, 0.0f, 0.6f};

} // namespace

TEST(PhosphorHeightTest, EqFactorIsOneWithNeutralGains) {
    const float eq[3] = {1.f, 1.f, 1.f};
    EXPECT_FLOAT_EQ(1.0f, eqHeightFactor(kLevels, eq));
}

TEST(PhosphorHeightTest, EqFactorIsTheNormRatioOfGainedLevels) {
    // levels (0.8, 0, 0.6) have norm 1.0; (0.4, 0, 0.6) has norm sqrt(0.52).
    const float eq[3] = {0.5f, 1.f, 1.f};
    EXPECT_NEAR(0.72111f, eqHeightFactor(kLevels, eq), 1e-4);
}

TEST(PhosphorHeightTest, ALowKillShrinksAKickAndLeavesAHighHatAlone) {
    const float kill[3] = {0.f, 1.f, 1.f};
    const float kick[3] = {1.0f, 0.1f, 0.0f};
    const float hat[3] = {0.0f, 0.1f, 1.0f};
    EXPECT_NEAR(0.0995f, eqHeightFactor(kick, kill), 1e-3);
    EXPECT_NEAR(1.0f, eqHeightFactor(hat, kill), 1e-6);
}

TEST(PhosphorHeightTest, EqFactorOfASilentColumnIsOne) {
    const float zero[3] = {0.f, 0.f, 0.f};
    const float eq[3] = {0.f, 0.f, 0.f};
    EXPECT_FLOAT_EQ(1.0f, eqHeightFactor(zero, eq));
}

TEST(PhosphorHeightTest, GainsFollowBoth) {
    Inputs in;
    in.totalGain = 2.0f;
    in.visualAll = 1.5f;
    in.lowKnob = 0.5f;
    in.visualLow = 2.0f;
    in.highKill = true;
    const ShaderGains g = shaderGains(in);
    EXPECT_FLOAT_EQ(3.0f, g.height);
    // The EQ gains don't include the visual gains; the color gains do.
    EXPECT_FLOAT_EQ(0.5f, g.eq[0]);
    EXPECT_FLOAT_EQ(1.0f, g.eq[1]);
    EXPECT_FLOAT_EQ(0.0f, g.eq[2]);
    EXPECT_FLOAT_EQ(1.0f, g.color[0]);
    EXPECT_FLOAT_EQ(0.0f, g.color[2]);
}

TEST(PhosphorHeightTest, BothOffLeavesOnlyTheVisualGains) {
    Inputs in;
    in.totalGain = 2.0f;
    in.visualAll = 1.5f;
    in.visualMid = 0.7f;
    in.lowKnob = 0.1f;
    in.lowKill = true;
    in.followsGain = false;
    in.followsEq = false;
    const ShaderGains g = shaderGains(in);
    EXPECT_FLOAT_EQ(1.5f, g.height);
    for (int i = 0; i < 3; ++i) {
        EXPECT_FLOAT_EQ(1.0f, g.eq[i]);
    }
    EXPECT_FLOAT_EQ(1.0f, g.color[0]);
    EXPECT_FLOAT_EQ(0.7f, g.color[1]);
}

TEST(PhosphorHeightTest, DisabledEqCountsAsNeutral) {
    Inputs in;
    in.lowKnob = 0.0f;
    in.eqEnabled = false;
    EXPECT_FLOAT_EQ(1.0f, shaderGains(in).eq[0]);
}

TEST(PhosphorHeightTest, ColumnHeightFactorCombinesGainAndEq) {
    ShaderGains g;
    g.height = 2.0f;
    g.eq[0] = 0.5f;
    EXPECT_NEAR(1.44222f, columnHeightFactor(kLevels, g), 1e-4);
}

TEST(PhosphorHeightTest, StretchResamplesAtYOverGAboutTheCenter) {
    // A lane of 100 rows, center at 50.0. Pixel row 70 (center 70.5) at g = 2
    // reads the source at 50 + 20.5 / 2 = 60.25, i.e. row 60.
    EXPECT_EQ(60, stretchSourceRow(70, 100, 2.0f));
    EXPECT_EQ(70, stretchSourceRow(70, 100, 1.0f));
    // g < 1 pulls from further out: 50 + 10.5 / 0.5 = 71.
    EXPECT_EQ(71, stretchSourceRow(60, 100, 0.5f));
}

TEST(PhosphorHeightTest, StretchClipsAtTheLaneEdge) {
    // g = 0.5: pixel row 80 reads 50 + 30.5 / 0.5 = 111: outside the lane.
    EXPECT_EQ(-1, stretchSourceRow(80, 100, 0.5f));
    EXPECT_EQ(-1, stretchSourceRow(5, 100, 0.5f));
    // No height at all draws nothing.
    EXPECT_EQ(-1, stretchSourceRow(70, 100, 0.0f));
}

TEST(PhosphorHeightTest, StretchedDensityIsDividedByG) {
    EXPECT_FLOAT_EQ(1.5f, stretchedDensity(3.0f, 2.0f));
    EXPECT_FLOAT_EQ(0.0f, stretchedDensity(3.0f, 0.0f));
}
