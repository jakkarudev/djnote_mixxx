#include <gtest/gtest.h>

#include "waveform/overviewlayout.h"

namespace {

using namespace mixxx::overviewlayout;

// 100 s track at 44100 Hz stereo, 1000 px overview (1 px = 0.1 s), play
// position at 50 %.
Input makeInput(TimeRulerState state, double pointerPos, double length = 1000.0) {
    Input input;
    input.sampleRate = 44100.0;
    input.trackSamples = 100.0 * 44100.0 * 2;
    input.rateRatio = 1.0;
    input.playPositionFraction = 0.5;
    input.pointerPos = pointerPos;
    input.overviewLength = length;
    input.timeRulerState = state;
    return input;
}

} // namespace

TEST(OverviewLayoutTimeRulerTest, InactiveShowsNothing) {
    const auto layout = layoutTimeRuler(makeInput(TimeRulerState::Inactive, 250));
    EXPECT_FALSE(layout.lineShown);
    EXPECT_FALSE(layout.labelsShown);
    EXPECT_TRUE(layout.positionLabelText.isEmpty());
    EXPECT_TRUE(layout.distanceLabelText.isEmpty());
}

TEST(OverviewLayoutTimeRulerTest, RightClickHoldShowsLineAndLabels) {
    const auto layout = layoutTimeRuler(makeInput(TimeRulerState::RightClickHold, 250));
    EXPECT_TRUE(layout.lineShown);
    EXPECT_DOUBLE_EQ(250.0, layout.linePos);
    EXPECT_TRUE(layout.labelsShown);
    EXPECT_EQ("0:25 -1:15", layout.positionLabelText);
    EXPECT_EQ("-0:25", layout.distanceLabelText);
}

TEST(OverviewLayoutTimeRulerTest, LeftDragShowsLabelsWithoutLine) {
    const auto layout = layoutTimeRuler(makeInput(TimeRulerState::LeftDrag, 750));
    EXPECT_FALSE(layout.lineShown);
    EXPECT_TRUE(layout.labelsShown);
    EXPECT_EQ("1:15 -0:25", layout.positionLabelText);
    EXPECT_EQ("0:25", layout.distanceLabelText);
}

TEST(OverviewLayoutTimeRulerTest, PointerIsClampedToOverviewBounds) {
    auto layout = layoutTimeRuler(makeInput(TimeRulerState::RightClickHold, -40));
    EXPECT_DOUBLE_EQ(0.0, layout.linePos);
    EXPECT_EQ("0:00 -1:40", layout.positionLabelText);

    layout = layoutTimeRuler(makeInput(TimeRulerState::RightClickHold, 1200));
    EXPECT_DOUBLE_EQ(1000.0, layout.linePos);
    EXPECT_EQ("1:40 -0:00", layout.positionLabelText);
}

TEST(OverviewLayoutTimeRulerTest, ZeroDistanceHasNoSign) {
    const auto layout = layoutTimeRuler(makeInput(TimeRulerState::RightClickHold, 500));
    EXPECT_EQ("0:00", layout.distanceLabelText);
}

TEST(OverviewLayoutTimeRulerTest, DistanceUnderOneSecondBehindHasNoMinusZero) {
    // 0.5 s before the play position
    const auto layout = layoutTimeRuler(makeInput(TimeRulerState::RightClickHold, 495));
    EXPECT_EQ("0:00", layout.distanceLabelText);
}

TEST(OverviewLayoutTimeRulerTest, DistanceBehindPlayPositionIsNegative) {
    // 1.2 s before the play position
    const auto layout = layoutTimeRuler(makeInput(TimeRulerState::RightClickHold, 488));
    EXPECT_EQ("-0:01", layout.distanceLabelText);
}

TEST(OverviewLayoutTimeRulerTest, RateRatioScalesTimes) {
    auto input = makeInput(TimeRulerState::RightClickHold, 250);
    input.rateRatio = 2.0;
    const auto layout = layoutTimeRuler(input);
    EXPECT_EQ("0:12 -0:37", layout.positionLabelText);
}

TEST(OverviewLayoutTimeRulerTest, OrientationOnlyChangesLength) {
    // A vertical overview is just a different length along the track axis:
    // 125/500 px is the same fraction as 250/1000 px.
    const auto vertical = layoutTimeRuler(makeInput(TimeRulerState::RightClickHold, 125, 500));
    const auto horizontal = layoutTimeRuler(makeInput(TimeRulerState::RightClickHold, 250, 1000));
    EXPECT_EQ(horizontal.positionLabelText, vertical.positionLabelText);
    EXPECT_EQ(horizontal.distanceLabelText, vertical.distanceLabelText);
    EXPECT_DOUBLE_EQ(125.0, vertical.linePos);
}
