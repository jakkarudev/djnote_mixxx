#include <gtest/gtest.h>

#include "audio/types.h"
#include "track/beats.h"
#include "track/bpm.h"
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

// ---------------------------------------------------------------------------
// Beat label
// ---------------------------------------------------------------------------

namespace {

using namespace mixxx;

constexpr auto kBpm = Bpm(120.0);
constexpr auto kSampleRate = audio::SampleRate(48000);
constexpr auto kStartPosition = audio::FramePos(400);
// 120 BPM at 48 kHz: one beat is 24000 frames.
const auto kConstTempoBeats = Beats(kStartPosition, kBpm, kSampleRate, QString());
// 8 beats at 120 BPM, then 16 beats at 60 BPM (48000 frames each), then
// 120 BPM.
const auto kNonConstTempoBeats = Beats(
        std::vector<BeatMarker>{
                BeatMarker{kStartPosition, 8},
                BeatMarker{kStartPosition + 8 * kSampleRate.value() / 2, 16},
        },
        kStartPosition + 8 * kSampleRate.value() / 2 + 16 * kSampleRate.value(),
        kBpm,
        kSampleRate,
        QString());

// 100 s track at 48 kHz, 1000 px overview: 1 px = 4800 frames = 0.2 beats
// (at 120 BPM).
constexpr double kTrackFrames = 100.0 * 48000;
constexpr double kOverviewLength = 1000.0;

struct BeatLabelCase {
    const Beats* pBeats = &kConstTempoBeats;
    // On beat 100, so that there is room before and after.
    double playFrame = 400 + 100 * 24000.0;
    double pointerFrame = 400 + 100 * 24000.0;
    TimeRulerState state = TimeRulerState::RightClickHold;
    HoverReadoutSettings settings;
    double labelHeight = 15.0;
    double breadth = 60.0;
    double length = kOverviewLength;

    Input input() const {
        Input in;
        in.trackSamples = kTrackFrames * 2;
        in.sampleRate = 48000.0;
        in.playPositionFraction = playFrame / kTrackFrames;
        in.pointerPos = pointerFrame / kTrackFrames * length;
        in.overviewLength = length;
        in.timeRulerState = state;
        in.pBeats = pBeats;
        in.hoverReadout = settings;
        in.labelHeight = labelHeight;
        in.overviewBreadth = breadth;
        return in;
    }
    BeatLabelLayout layout() const {
        return layoutBeatLabel(input());
    }
    // Pointer `beats` beats after the play position on the constant grid.
    BeatLabelCase& beatsAway(double beats) {
        pointerFrame = playFrame + beats * 24000.0;
        return *this;
    }
};

QString textFor(double beats,
        BeatLabelUnit unit = BeatLabelUnit::Bars,
        BeatLabelPrecision precision = BeatLabelPrecision::Whole) {
    BeatLabelCase c;
    c.settings.unit = unit;
    c.settings.precision = precision;
    c.beatsAway(beats);
    return c.layout().text;
}

} // namespace

TEST(OverviewLayoutBeatLabelTest, FourBarsAheadOnConstantGrid) {
    BeatLabelCase c;
    c.beatsAway(16);
    const auto layout = c.layout();
    EXPECT_TRUE(layout.shown);
    EXPECT_EQ("+4 bars", layout.text);
    EXPECT_EQ(LabelSide::Right, layout.side);
    EXPECT_EQ(LabelSlot::OwnBottomRow, layout.slot);
}

TEST(OverviewLayoutBeatLabelTest, NegativeDistance) {
    EXPECT_EQ("-4 bars", textFor(-16));
}

TEST(OverviewLayoutBeatLabelTest, ZeroHasNoSign) {
    EXPECT_EQ("0 bars", textFor(0));
    // Rounds to zero from either side: never "-0" or "+0".
    EXPECT_EQ("0 bars", textFor(-0.4));
    EXPECT_EQ("0 bars", textFor(0.4));
    EXPECT_EQ("0 beats", textFor(-0.3, BeatLabelUnit::Beats));
    EXPECT_EQ("0.00 bars",
            textFor(-0.001, BeatLabelUnit::Bars, BeatLabelPrecision::Fractional));
    EXPECT_EQ("0.0 beats",
            textFor(-0.01, BeatLabelUnit::Beats, BeatLabelPrecision::Fractional));
}

TEST(OverviewLayoutBeatLabelTest, PluralsUseSingularOnlyForOne) {
    EXPECT_EQ("+1 bar", textFor(4));
    EXPECT_EQ("-1 bar", textFor(-4));
    EXPECT_EQ("+2 bars", textFor(8));
    EXPECT_EQ("0 bars", textFor(0));
    EXPECT_EQ("+1 beat", textFor(1, BeatLabelUnit::Beats));
    EXPECT_EQ("+2 beats", textFor(2, BeatLabelUnit::Beats));
    EXPECT_EQ("0 beats", textFor(0, BeatLabelUnit::Beats));
}

TEST(OverviewLayoutBeatLabelTest, WholePrecisionRoundsToNearest) {
    // 5 beats = 1.25 bars, 7 beats = 1.75 bars
    EXPECT_EQ("+1 bar", textFor(5));
    EXPECT_EQ("+2 bars", textFor(7));
    EXPECT_EQ("-2 bars", textFor(-7));
    EXPECT_EQ("+5 beats", textFor(5.4, BeatLabelUnit::Beats));
    EXPECT_EQ("+6 beats", textFor(5.6, BeatLabelUnit::Beats));
}

TEST(OverviewLayoutBeatLabelTest, FractionalPrecisionDecimals) {
    using U = BeatLabelUnit;
    using P = BeatLabelPrecision;
    EXPECT_EQ("+1.25 bars", textFor(5, U::Bars, P::Fractional));
    EXPECT_EQ("-1.25 bars", textFor(-5, U::Bars, P::Fractional));
    EXPECT_EQ("+4.00 bars", textFor(16, U::Bars, P::Fractional));
    EXPECT_EQ("+5.5 beats", textFor(5.5, U::Beats, P::Fractional));
    EXPECT_EQ("+5.0 beats", textFor(5, U::Beats, P::Fractional));
}

TEST(OverviewLayoutBeatLabelTest, FractionalPositionInsideBeat) {
    BeatLabelCase c;
    c.settings.unit = BeatLabelUnit::Beats;
    c.settings.precision = BeatLabelPrecision::Fractional;
    c.playFrame = 400 + 3 * 24000.0;
    c.beatsAway(1.3);
    EXPECT_EQ("+1.3 beats", c.layout().text);
    c.beatsAway(0.1);
    EXPECT_EQ("+0.1 beats", c.layout().text);
}

TEST(OverviewLayoutBeatLabelTest, PlayPositionInsideBeatIsMeasuredFromThere) {
    BeatLabelCase c;
    c.settings.unit = BeatLabelUnit::Beats;
    c.settings.precision = BeatLabelPrecision::Fractional;
    c.playFrame = 400 + 12000.0; // half a beat after the first beat
    c.beatsAway(2);
    EXPECT_EQ("+2.0 beats", c.layout().text);
}

TEST(OverviewLayoutBeatLabelTest, PointerBeforeFirstBeat) {
    // Frame 0 is 1/60 beat before the first beat, 100 beats before the play
    // position at 50 %.
    BeatLabelCase c;
    c.playFrame = kTrackFrames / 2;
    c.pointerFrame = 0;
    EXPECT_EQ("-25 bars", c.layout().text);
}

TEST(OverviewLayoutBeatLabelTest, PointerAfterLastBeat) {
    // The constant grid extrapolates after its last marker.
    BeatLabelCase c;
    c.playFrame = kTrackFrames / 2;
    c.pointerFrame = kTrackFrames;
    c.settings.position = BeatLabelPosition::AlwaysRight;
    EXPECT_EQ("+25 bars", c.layout().text);
}

TEST(OverviewLayoutBeatLabelTest, PlayPositionBeforeFirstBeat) {
    BeatLabelCase c;
    c.playFrame = 0;
    c.pointerFrame = 400 + 8 * 24000.0;
    // (8 + 1/60) beats after frame 0, rounds to 2 bars
    EXPECT_EQ("+2 bars", c.layout().text);
}

TEST(OverviewLayoutBeatLabelTest, TempoChangeIsCountedAlongBeats) {
    // Beat 4 (120 BPM section) to beat 10 (two beats into the 60 BPM
    // section): 6 beats, although it spans more than 6 * 24000 frames.
    BeatLabelCase c;
    c.pBeats = &kNonConstTempoBeats;
    c.settings.unit = BeatLabelUnit::Beats;
    c.playFrame = 400 + 4 * 24000.0;
    c.pointerFrame = 400 + 8 * 24000.0 + 2 * 48000.0;
    EXPECT_EQ("+6 beats", c.layout().text);
    // Half a beat further inside the slow section.
    c.pointerFrame += 24000.0;
    c.settings.precision = BeatLabelPrecision::Fractional;
    EXPECT_EQ("+6.5 beats", c.layout().text);
    // And backwards, from the slow section into the fast one.
    c.playFrame = 400 + 8 * 24000.0 + 4 * 48000.0; // beat 12
    c.pointerFrame = 400 + 2 * 24000.0;            // beat 2
    EXPECT_EQ("-10.0 beats", c.layout().text);
}

TEST(OverviewLayoutBeatLabelTest, BarsAreAQuarterOfBeats) {
    EXPECT_EQ("+2 bars", textFor(8));
    EXPECT_EQ("+8 beats", textFor(8, BeatLabelUnit::Beats));
}

TEST(OverviewLayoutBeatLabelTest, TextOrderPerSide) {
    BeatLabelCase c;
    c.beatsAway(16);
    c.settings.position = BeatLabelPosition::AlwaysRight;
    EXPECT_EQ("+4 bars", c.layout().text);
    c.settings.position = BeatLabelPosition::AlwaysLeft;
    const auto layout = c.layout();
    EXPECT_EQ(LabelSide::Left, layout.side);
    EXPECT_EQ("bars +4", layout.text);
    c.beatsAway(4);
    EXPECT_EQ("bar +1", c.layout().text);
    c.beatsAway(0);
    EXPECT_EQ("bars 0", c.layout().text);
    c.beatsAway(-8);
    EXPECT_EQ("bars -2", c.layout().text);
    c.settings.unit = BeatLabelUnit::Beats;
    EXPECT_EQ("beats -8", c.layout().text);
}

TEST(OverviewLayoutBeatLabelTest, AutoSideSwitchesPastThreeQuarters) {
    BeatLabelCase c;
    c.settings.position = BeatLabelPosition::Auto;
    c.pointerFrame = kTrackFrames * 0.7499;
    EXPECT_EQ(LabelSide::Right, c.layout().side);
    c.pointerFrame = kTrackFrames * 0.75;
    EXPECT_EQ(LabelSide::Right, c.layout().side);
    c.pointerFrame = kTrackFrames * 0.7501;
    EXPECT_EQ(LabelSide::Left, c.layout().side);
    c.pointerFrame = kTrackFrames;
    EXPECT_EQ(LabelSide::Left, c.layout().side);
    c.pointerFrame = 0;
    EXPECT_EQ(LabelSide::Right, c.layout().side);
}

TEST(OverviewLayoutBeatLabelTest, AutoSideUsesFractionOfOverviewLength) {
    // Vertical overview of 500 px: same fractions.
    BeatLabelCase c;
    c.length = 500.0;
    c.pointerFrame = kTrackFrames * 0.74;
    EXPECT_EQ(LabelSide::Right, c.layout().side);
    c.pointerFrame = kTrackFrames * 0.76;
    EXPECT_EQ(LabelSide::Left, c.layout().side);
}

TEST(OverviewLayoutBeatLabelTest, AlwaysRightAndAlwaysLeftOverrideAuto) {
    BeatLabelCase c;
    c.pointerFrame = kTrackFrames * 0.9;
    c.settings.position = BeatLabelPosition::AlwaysRight;
    EXPECT_EQ(LabelSide::Right, c.layout().side);
    c.pointerFrame = kTrackFrames * 0.1;
    c.settings.position = BeatLabelPosition::AlwaysLeft;
    EXPECT_EQ(LabelSide::Left, c.layout().side);
}

TEST(OverviewLayoutBeatLabelTest, ThreeRowsFitGivesOwnRow) {
    BeatLabelCase c;
    c.labelHeight = 15.0;
    c.breadth = 45.0; // exactly three rows
    c.beatsAway(16);
    const auto layout = c.layout();
    EXPECT_EQ(LabelSlot::OwnBottomRow, layout.slot);
    EXPECT_TRUE(layout.joinedDistanceLabelText.isEmpty());
}

TEST(OverviewLayoutBeatLabelTest, ShortOverviewJoinsTimeDistanceLabel) {
    BeatLabelCase c;
    c.labelHeight = 15.0;
    c.breadth = 44.9;
    c.beatsAway(17); // 8.5 s at 120 BPM
    auto layout = c.layout();
    EXPECT_TRUE(layout.shown);
    EXPECT_EQ(LabelSlot::JoinedToDistanceLabel, layout.slot);
    EXPECT_EQ("+4 bars", layout.text);
    EXPECT_EQ("0:08  +4 bars", layout.joinedDistanceLabelText);

    c.beatsAway(-17);
    EXPECT_EQ("-0:08  -4 bars", c.layout().joinedDistanceLabelText);
}

TEST(OverviewLayoutBeatLabelTest, JoinedLabelStaysOnTheRightOfTheLine) {
    BeatLabelCase c;
    c.breadth = 10.0;
    c.settings.position = BeatLabelPosition::AlwaysLeft;
    c.beatsAway(17);
    const auto layout = c.layout();
    EXPECT_EQ(LabelSide::Right, layout.side);
    EXPECT_EQ("0:08  +4 bars", layout.joinedDistanceLabelText);
}

TEST(OverviewLayoutBeatLabelTest, NoBeatGridNoLabel) {
    BeatLabelCase c;
    c.pBeats = nullptr;
    c.beatsAway(17);
    const auto layout = c.layout();
    EXPECT_FALSE(layout.shown);
    EXPECT_TRUE(layout.text.isEmpty());
    // The time labels are unaffected.
    EXPECT_EQ("0:08", layoutTimeRuler(c.input()).distanceLabelText);
}

TEST(OverviewLayoutBeatLabelTest, InactiveTimeRulerShowsNoLabel) {
    BeatLabelCase c;
    c.state = TimeRulerState::Inactive;
    c.beatsAway(16);
    EXPECT_FALSE(c.layout().shown);
}

TEST(OverviewLayoutBeatLabelTest, LeftDragAlsoShowsLabel) {
    BeatLabelCase c;
    c.state = TimeRulerState::LeftDrag;
    c.beatsAway(16);
    EXPECT_TRUE(c.layout().shown);
    EXPECT_EQ("+4 bars", c.layout().text);
}

TEST(OverviewLayoutBeatLabelTest, PointerIsClampedToOverview) {
    BeatLabelCase c;
    c.playFrame = kTrackFrames / 2;
    c.pointerFrame = kTrackFrames * 2; // beyond the end
    c.settings.position = BeatLabelPosition::AlwaysRight;
    EXPECT_EQ("+25 bars", c.layout().text);
}

TEST(OverviewLayoutBeatLabelTest, UnloadedTrackShowsNoLabel) {
    BeatLabelCase c;
    auto input = c.input();
    input.trackSamples = 0.0;
    EXPECT_FALSE(layoutBeatLabel(input).shown);
    input = c.input();
    input.overviewLength = 0.0;
    EXPECT_FALSE(layoutBeatLabel(input).shown);
}
