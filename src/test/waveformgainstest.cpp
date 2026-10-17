#include <gtest/gtest.h>

#include "waveform/waveformgains.h"

namespace {

using namespace mixxx::waveformgains;

} // namespace

TEST(WaveformGainsTest, NeutralInputsGiveUnityGains) {
    const Gains g = calculateGains(Inputs{});
    EXPECT_FLOAT_EQ(1.0f, g.all);
    EXPECT_FLOAT_EQ(1.0f, g.low);
    EXPECT_FLOAT_EQ(1.0f, g.mid);
    EXPECT_FLOAT_EQ(1.0f, g.high);
}

TEST(WaveformGainsTest, FollowsGainUsesTotalGainWithOptionalCompensation) {
    Inputs in;
    in.totalGain = 0.5f;
    in.visualAll = 1.5f;
    EXPECT_FLOAT_EQ(0.75f, calculateGains(in).all);
    in.applyCompensation = true;
    EXPECT_FLOAT_EQ(1.5f, calculateGains(in).all);
}

TEST(WaveformGainsTest, FollowsGainOffTreatsTotalGainAsOneButKeepsCompensation) {
    Inputs in;
    in.totalGain = 0.5f;
    in.visualAll = 1.5f;
    in.followsGain = false;
    EXPECT_FLOAT_EQ(1.5f, calculateGains(in).all);
    in.applyCompensation = true;
    EXPECT_FLOAT_EQ(3.0f, calculateGains(in).all);
}

TEST(WaveformGainsTest, FollowsGainDoesNotAffectBands) {
    Inputs in;
    in.totalGain = 0.25f;
    in.lowKnob = 0.5f;
    const Gains on = calculateGains(in);
    in.followsGain = false;
    const Gains off = calculateGains(in);
    EXPECT_FLOAT_EQ(0.5f, on.low);
    EXPECT_FLOAT_EQ(0.5f, off.low);
}

TEST(WaveformGainsTest, EqKnobsAndVisualGainsScaleBandsWhenFollowingEq) {
    Inputs in;
    in.lowKnob = 0.5f;
    in.midKnob = 2.0f;
    in.highKnob = 0.0f;
    in.visualLow = 2.0f;
    in.visualMid = 0.5f;
    in.visualHigh = 3.0f;
    const Gains g = calculateGains(in);
    EXPECT_FLOAT_EQ(1.0f, g.low);
    EXPECT_FLOAT_EQ(1.0f, g.mid);
    EXPECT_FLOAT_EQ(0.0f, g.high);
}

TEST(WaveformGainsTest, EachKillZeroesOnlyItsBand) {
    Inputs in;
    in.visualLow = 2.0f;
    in.visualMid = 2.0f;
    in.visualHigh = 2.0f;

    in.lowKill = true;
    Gains g = calculateGains(in);
    EXPECT_FLOAT_EQ(0.0f, g.low);
    EXPECT_FLOAT_EQ(2.0f, g.mid);
    EXPECT_FLOAT_EQ(2.0f, g.high);

    in.lowKill = false;
    in.midKill = true;
    g = calculateGains(in);
    EXPECT_FLOAT_EQ(2.0f, g.low);
    EXPECT_FLOAT_EQ(0.0f, g.mid);
    EXPECT_FLOAT_EQ(2.0f, g.high);

    in.midKill = false;
    in.highKill = true;
    g = calculateGains(in);
    EXPECT_FLOAT_EQ(2.0f, g.low);
    EXPECT_FLOAT_EQ(2.0f, g.mid);
    EXPECT_FLOAT_EQ(0.0f, g.high);
}

TEST(WaveformGainsTest, FollowsEqOffIgnoresKnobsAndKillsButKeepsVisualGains) {
    Inputs in;
    in.followsEq = false;
    in.lowKnob = 0.1f;
    in.midKnob = 0.1f;
    in.highKnob = 0.1f;
    in.lowKill = true;
    in.midKill = true;
    in.highKill = true;
    in.visualLow = 2.0f;
    in.visualMid = 3.0f;
    in.visualHigh = 4.0f;
    const Gains g = calculateGains(in);
    EXPECT_FLOAT_EQ(2.0f, g.low);
    EXPECT_FLOAT_EQ(3.0f, g.mid);
    EXPECT_FLOAT_EQ(4.0f, g.high);
}

TEST(WaveformGainsTest, DisabledDeckEqIgnoresKnobsAndKillsButKeepsVisualGains) {
    Inputs in;
    in.eqEnabled = false;
    in.lowKnob = 0.1f;
    in.highKill = true;
    in.visualLow = 2.0f;
    in.visualMid = 3.0f;
    in.visualHigh = 4.0f;
    const Gains g = calculateGains(in);
    EXPECT_FLOAT_EQ(2.0f, g.low);
    EXPECT_FLOAT_EQ(3.0f, g.mid);
    EXPECT_FLOAT_EQ(4.0f, g.high);
}

TEST(WaveformGainsTest, AllCombinationsOfFlags) {
    for (const bool followsGain : {false, true}) {
        for (const bool followsEq : {false, true}) {
            for (const bool eqEnabled : {false, true}) {
                for (const bool comp : {false, true}) {
                    Inputs in;
                    in.followsGain = followsGain;
                    in.followsEq = followsEq;
                    in.eqEnabled = eqEnabled;
                    in.applyCompensation = comp;
                    in.totalGain = 0.5f;
                    in.lowKnob = 0.5f;
                    in.midKill = true;
                    in.visualAll = 2.0f;
                    in.visualLow = 4.0f;
                    in.visualMid = 4.0f;
                    in.visualHigh = 4.0f;
                    const Gains g = calculateGains(in);
                    const float expectedAll =
                            (followsGain ? 0.5f : 1.0f) * (comp ? 2.0f : 1.0f) * 2.0f;
                    const bool eqActive = followsEq && eqEnabled;
                    EXPECT_FLOAT_EQ(expectedAll, g.all);
                    EXPECT_FLOAT_EQ(eqActive ? 2.0f : 4.0f, g.low);
                    EXPECT_FLOAT_EQ(eqActive ? 0.0f : 4.0f, g.mid);
                    EXPECT_FLOAT_EQ(4.0f, g.high);
                }
            }
        }
    }
}
