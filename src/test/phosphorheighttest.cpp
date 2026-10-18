#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <vector>

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

namespace {

std::vector<float> stretchColumn(const std::vector<float>& src, float g) {
    const int h = static_cast<int>(src.size());
    std::vector<float> out(h);
    for (int y = 0; y < h; ++y) {
        out[y] = stretchedRowDensity(src.data(), h, y, g);
    }
    return out;
}

float sum(const std::vector<float>& v) {
    return std::accumulate(v.begin(), v.end(), 0.0f);
}

// A hollow box: bright 1-row edges at 40 and 59, a dim inside.
std::vector<float> hollowBox() {
    std::vector<float> src(100, 0.0f);
    for (int y = 40; y <= 59; ++y) {
        src[y] = 0.1f;
    }
    src[40] = 5.0f;
    src[59] = 5.0f;
    return src;
}

} // namespace

TEST(PhosphorHeightTest, StretchByOneIsTheIdentity) {
    const std::vector<float> src = hollowBox();
    EXPECT_EQ(src, stretchColumn(src, 1.0f));
}

TEST(PhosphorHeightTest, StretchUpMatchesTheSourceAtYOverGDividedByG) {
    // g = 2: pixel row 70 (center 70.5) covers the source [60, 60.5], inside
    // row 60.
    std::vector<float> src(100, 0.0f);
    src[60] = 3.0f;
    src[61] = 7.0f;
    EXPECT_FLOAT_EQ(1.5f, stretchedRowDensity(src.data(), 100, 70, 2.0f));
    EXPECT_FLOAT_EQ(1.5f, stretchedRowDensity(src.data(), 100, 71, 2.0f));
    EXPECT_FLOAT_EQ(3.5f, stretchedRowDensity(src.data(), 100, 72, 2.0f));
}

TEST(PhosphorHeightTest, ShrinkKeepsAOneRowEdgeAndTheTotal) {
    const std::vector<float> src = hollowBox();
    const std::vector<float> out = stretchColumn(src, 0.4f);
    EXPECT_NEAR(sum(src), sum(out), 1e-3);
    // The edges survive as the brightest rows, and the inside stays dark.
    const float edge = *std::max_element(out.begin(), out.end());
    EXPECT_GT(edge, 1.5f);
    const int top = static_cast<int>(std::max_element(out.begin(), out.begin() + 50) - out.begin());
    EXPECT_GT(out[top], 1.5f);
    EXPECT_NEAR(0.0f, out[50], 0.3f * edge);
    EXPECT_LT(out[50], 0.25f * edge);
    // The box shrank towards the center: 20 rows * 0.4 = 8 rows.
    EXPECT_EQ(0.0f, out[30]);
    EXPECT_EQ(0.0f, out[70]);
}

TEST(PhosphorHeightTest, TheTotalIsConservedForEveryGInsideTheLane) {
    const std::vector<float> src = hollowBox();
    for (float g : {0.07f, 0.1f, 0.25f, 0.4f, 0.5f, 0.77f, 0.99f, 1.0f, 1.3f, 2.0f}) {
        EXPECT_NEAR(sum(src), sum(stretchColumn(src, g)), 1e-3) << "g = " << g;
    }
}

TEST(PhosphorHeightTest, StretchClipsAtTheLaneEdge) {
    // Density in the outer rows of the lane is pushed out of it when g > 1.
    std::vector<float> src(100, 0.0f);
    src[0] = 4.0f;
    src[99] = 4.0f;
    src[50] = 1.0f;
    const std::vector<float> out = stretchColumn(src, 2.0f);
    EXPECT_FLOAT_EQ(0.0f, out[0]);
    EXPECT_FLOAT_EQ(0.0f, out[99]);
    EXPECT_NEAR(1.0f, sum(out), 1e-5);
    // g < 1: pixel row 80 covers the source [110, 112]: outside the lane.
    std::vector<float> full(100, 1.0f);
    EXPECT_FLOAT_EQ(0.0f, stretchedRowDensity(full.data(), 100, 80, 0.5f));
    EXPECT_FLOAT_EQ(0.0f, stretchedRowDensity(full.data(), 100, 5, 0.5f));
}

TEST(PhosphorHeightTest, NoHeightAtAllDrawsNothing) {
    const std::vector<float> src(100, 1.0f);
    EXPECT_FLOAT_EQ(0.0f, stretchedRowDensity(src.data(), 100, 70, 0.0f));
    EXPECT_FLOAT_EQ(0.0f, stretchedRowDensity(src.data(), 100, 70, kMinHeightFactor / 2));
}
