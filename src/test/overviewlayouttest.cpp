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
// Hover readout (beat distance)
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

struct HoverReadoutCase {
    const Beats* pBeats = &kConstTempoBeats;
    // On beat 100, so that there is room before and after.
    double playFrame = 400 + 100 * 24000.0;
    double pointerFrame = 400 + 100 * 24000.0;
    TimeRulerState state = TimeRulerState::RightClickHold;
    HoverReadoutSettings settings;
    double labelHeight = 15.0;
    double breadth = 60.0;
    double length = kOverviewLength;

    // The tests of the hover readout predate snapped seeks and describe the
    // overview with the setting off.
    HoverReadoutCase() {
        settings.snapSeeks = false;
    }

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
    HoverReadoutLayout layout() const {
        return layoutHoverReadout(input());
    }
    // Pointer `beats` beats after the play position on the constant grid.
    HoverReadoutCase& pointerBeatDistance(double beats) {
        pointerFrame = playFrame + beats * 24000.0;
        return *this;
    }
};

QString textFor(double beats,
        BeatDistanceUnit unit = BeatDistanceUnit::Bars,
        BeatDistancePrecision precision = BeatDistancePrecision::Whole) {
    HoverReadoutCase c;
    c.settings.unit = unit;
    c.settings.precision = precision;
    c.pointerBeatDistance(beats);
    return c.layout().text;
}

} // namespace

TEST(OverviewLayoutBeatDistanceTest, FourBarsAheadOnConstantGrid) {
    HoverReadoutCase c;
    c.pointerBeatDistance(16);
    const auto layout = c.layout();
    EXPECT_TRUE(layout.shown);
    EXPECT_EQ("+4 bars", layout.text);
    EXPECT_EQ(LabelSide::Right, layout.side);
    EXPECT_EQ(LabelSlot::OwnBottomRow, layout.slot);
}

TEST(OverviewLayoutBeatDistanceTest, NegativeDistance) {
    EXPECT_EQ("-4 bars", textFor(-16));
}

TEST(OverviewLayoutBeatDistanceTest, ZeroHasNoSign) {
    EXPECT_EQ("0 bars", textFor(0));
    // Rounds to zero from either side: never "-0" or "+0".
    EXPECT_EQ("0 bars", textFor(-0.4));
    EXPECT_EQ("0 bars", textFor(0.4));
    EXPECT_EQ("0 beats", textFor(-0.3, BeatDistanceUnit::Beats));
    EXPECT_EQ("0.00 bars",
            textFor(-0.001, BeatDistanceUnit::Bars, BeatDistancePrecision::Fractional));
    EXPECT_EQ("0.0 beats",
            textFor(-0.01, BeatDistanceUnit::Beats, BeatDistancePrecision::Fractional));
}

TEST(OverviewLayoutBeatDistanceTest, PluralsUseSingularOnlyForOne) {
    EXPECT_EQ("+1 bar", textFor(4));
    EXPECT_EQ("-1 bar", textFor(-4));
    EXPECT_EQ("+2 bars", textFor(8));
    EXPECT_EQ("0 bars", textFor(0));
    EXPECT_EQ("+1 beat", textFor(1, BeatDistanceUnit::Beats));
    EXPECT_EQ("+2 beats", textFor(2, BeatDistanceUnit::Beats));
    EXPECT_EQ("0 beats", textFor(0, BeatDistanceUnit::Beats));
}

TEST(OverviewLayoutBeatDistanceTest, FractionalValuesAreNeverSingular) {
    using P = BeatDistancePrecision;
    EXPECT_EQ("+1.00 bars", textFor(4, BeatDistanceUnit::Bars, P::Fractional));
    EXPECT_EQ("+1.25 bars", textFor(5, BeatDistanceUnit::Bars, P::Fractional));
    EXPECT_EQ("+1.0 beats", textFor(1, BeatDistanceUnit::Beats, P::Fractional));
}

// Beat distance is counted along the beat grid: the playback rate (and with
// it sync and loops, which only change the rate or the play position) does not
// change it.
TEST(OverviewLayoutBeatDistanceTest, UnaffectedByRateRatio) {
    for (const auto unit : {BeatDistanceUnit::Bars, BeatDistanceUnit::Beats}) {
        for (const auto side : {HoverReadoutPosition::AlwaysRight,
                     HoverReadoutPosition::AlwaysLeft}) {
            HoverReadoutCase c;
            c.settings.unit = unit;
            c.settings.position = side;
            c.pointerBeatDistance(12);
            const QString expected = c.layout().text;
            for (const double rateRatio : {0.5, 0.92, 1.08, 1.5, 2.0}) {
                Input in = c.input();
                in.rateRatio = rateRatio;
                EXPECT_EQ(expected, layoutHoverReadout(in).text) << rateRatio;
            }
        }
    }
}

TEST(OverviewLayoutBeatDistanceTest, WholePrecisionRoundsToNearest) {
    // 5 beats = 1.25 bars, 7 beats = 1.75 bars
    EXPECT_EQ("+1 bar", textFor(5));
    EXPECT_EQ("+2 bars", textFor(7));
    EXPECT_EQ("-2 bars", textFor(-7));
    EXPECT_EQ("+5 beats", textFor(5.4, BeatDistanceUnit::Beats));
    EXPECT_EQ("+6 beats", textFor(5.6, BeatDistanceUnit::Beats));
}

TEST(OverviewLayoutBeatDistanceTest, FractionalPrecisionDecimals) {
    using U = BeatDistanceUnit;
    using P = BeatDistancePrecision;
    EXPECT_EQ("+1.25 bars", textFor(5, U::Bars, P::Fractional));
    EXPECT_EQ("-1.25 bars", textFor(-5, U::Bars, P::Fractional));
    EXPECT_EQ("+4.00 bars", textFor(16, U::Bars, P::Fractional));
    EXPECT_EQ("+5.5 beats", textFor(5.5, U::Beats, P::Fractional));
    EXPECT_EQ("+5.0 beats", textFor(5, U::Beats, P::Fractional));
}

TEST(OverviewLayoutBeatDistanceTest, FractionalPositionInsideBeat) {
    HoverReadoutCase c;
    c.settings.unit = BeatDistanceUnit::Beats;
    c.settings.precision = BeatDistancePrecision::Fractional;
    c.playFrame = 400 + 3 * 24000.0;
    c.pointerBeatDistance(1.3);
    EXPECT_EQ("+1.3 beats", c.layout().text);
    c.pointerBeatDistance(0.1);
    EXPECT_EQ("+0.1 beats", c.layout().text);
}

TEST(OverviewLayoutBeatDistanceTest, PlayPositionInsideBeatIsMeasuredFromThere) {
    HoverReadoutCase c;
    c.settings.unit = BeatDistanceUnit::Beats;
    c.settings.precision = BeatDistancePrecision::Fractional;
    c.playFrame = 400 + 12000.0; // half a beat after the first beat
    c.pointerBeatDistance(2);
    EXPECT_EQ("+2.0 beats", c.layout().text);
}

TEST(OverviewLayoutBeatDistanceTest, PointerBeforeFirstBeat) {
    // Frame 0 is 1/60 beat before the first beat, 100 beats before the play
    // position at 50 %.
    HoverReadoutCase c;
    c.playFrame = kTrackFrames / 2;
    c.pointerFrame = 0;
    EXPECT_EQ("-25 bars", c.layout().text);
}

TEST(OverviewLayoutBeatDistanceTest, PointerAfterLastBeat) {
    // The constant grid extrapolates after its last marker.
    HoverReadoutCase c;
    c.playFrame = kTrackFrames / 2;
    c.pointerFrame = kTrackFrames;
    c.settings.position = HoverReadoutPosition::AlwaysRight;
    EXPECT_EQ("+25 bars", c.layout().text);
}

TEST(OverviewLayoutBeatDistanceTest, PlayPositionBeforeFirstBeat) {
    HoverReadoutCase c;
    c.playFrame = 0;
    c.pointerFrame = 400 + 8 * 24000.0;
    // (8 + 1/60) beats after frame 0, rounds to 2 bars
    EXPECT_EQ("+2 bars", c.layout().text);
}

TEST(OverviewLayoutBeatDistanceTest, TempoChangeIsCountedAlongBeats) {
    // Beat 4 (120 BPM section) to beat 10 (two beats into the 60 BPM
    // section): 6 beats, although it spans more than 6 * 24000 frames.
    HoverReadoutCase c;
    c.pBeats = &kNonConstTempoBeats;
    c.settings.unit = BeatDistanceUnit::Beats;
    c.playFrame = 400 + 4 * 24000.0;
    c.pointerFrame = 400 + 8 * 24000.0 + 2 * 48000.0;
    EXPECT_EQ("+6 beats", c.layout().text);
    // Half a beat further inside the slow section.
    c.pointerFrame += 24000.0;
    c.settings.precision = BeatDistancePrecision::Fractional;
    EXPECT_EQ("+6.5 beats", c.layout().text);
    // And backwards, from the slow section into the fast one.
    c.playFrame = 400 + 8 * 24000.0 + 4 * 48000.0; // beat 12
    c.pointerFrame = 400 + 2 * 24000.0;            // beat 2
    EXPECT_EQ("-10.0 beats", c.layout().text);
}

TEST(OverviewLayoutBeatDistanceTest, BarsAreAQuarterOfBeats) {
    EXPECT_EQ("+2 bars", textFor(8));
    EXPECT_EQ("+8 beats", textFor(8, BeatDistanceUnit::Beats));
}

TEST(OverviewLayoutBeatDistanceTest, TextOrderPerSide) {
    HoverReadoutCase c;
    c.pointerBeatDistance(16);
    c.settings.position = HoverReadoutPosition::AlwaysRight;
    EXPECT_EQ("+4 bars", c.layout().text);
    c.settings.position = HoverReadoutPosition::AlwaysLeft;
    const auto layout = c.layout();
    EXPECT_EQ(LabelSide::Left, layout.side);
    EXPECT_EQ("bars +4", layout.text);
    c.pointerBeatDistance(4);
    EXPECT_EQ("bar +1", c.layout().text);
    c.pointerBeatDistance(0);
    EXPECT_EQ("bars 0", c.layout().text);
    c.pointerBeatDistance(-8);
    EXPECT_EQ("bars -2", c.layout().text);
    c.settings.unit = BeatDistanceUnit::Beats;
    EXPECT_EQ("beats -8", c.layout().text);
}

TEST(OverviewLayoutBeatDistanceTest, AutoSideSwitchesPastThreeQuarters) {
    HoverReadoutCase c;
    c.settings.position = HoverReadoutPosition::Auto;
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

TEST(OverviewLayoutBeatDistanceTest, AutoSideUsesFractionOfOverviewLength) {
    // Vertical overview of 500 px: same fractions.
    HoverReadoutCase c;
    c.length = 500.0;
    c.pointerFrame = kTrackFrames * 0.74;
    EXPECT_EQ(LabelSide::Right, c.layout().side);
    c.pointerFrame = kTrackFrames * 0.76;
    EXPECT_EQ(LabelSide::Left, c.layout().side);
}

TEST(OverviewLayoutBeatDistanceTest, AlwaysRightAndAlwaysLeftOverrideAuto) {
    HoverReadoutCase c;
    c.pointerFrame = kTrackFrames * 0.9;
    c.settings.position = HoverReadoutPosition::AlwaysRight;
    EXPECT_EQ(LabelSide::Right, c.layout().side);
    c.pointerFrame = kTrackFrames * 0.1;
    c.settings.position = HoverReadoutPosition::AlwaysLeft;
    EXPECT_EQ(LabelSide::Left, c.layout().side);
}

TEST(OverviewLayoutBeatDistanceTest, ThreeRowsFitGivesOwnRow) {
    HoverReadoutCase c;
    c.labelHeight = 15.0;
    c.breadth = 45.0; // exactly three rows
    c.pointerBeatDistance(16);
    const auto layout = c.layout();
    EXPECT_EQ(LabelSlot::OwnBottomRow, layout.slot);
    EXPECT_TRUE(layout.joinedDistanceLabelText.isEmpty());
}

TEST(OverviewLayoutBeatDistanceTest, ShortOverviewJoinsTimeDistanceLabel) {
    HoverReadoutCase c;
    c.labelHeight = 15.0;
    c.breadth = 44.9;
    c.pointerBeatDistance(17); // 8.5 s at 120 BPM
    auto layout = c.layout();
    EXPECT_TRUE(layout.shown);
    EXPECT_EQ(LabelSlot::JoinedToTimeRulerDistanceLabel, layout.slot);
    EXPECT_EQ("+4 bars", layout.text);
    EXPECT_EQ("0:08  +4 bars", layout.joinedDistanceLabelText);

    c.pointerBeatDistance(-17);
    EXPECT_EQ("-0:08  -4 bars", c.layout().joinedDistanceLabelText);
}

TEST(OverviewLayoutBeatDistanceTest, JoinedLabelStaysOnTheRightOfTheLine) {
    HoverReadoutCase c;
    c.breadth = 10.0;
    c.settings.position = HoverReadoutPosition::AlwaysLeft;
    c.pointerBeatDistance(17);
    const auto layout = c.layout();
    EXPECT_EQ(LabelSide::Right, layout.side);
    EXPECT_EQ("0:08  +4 bars", layout.joinedDistanceLabelText);
}

TEST(OverviewLayoutBeatDistanceTest, NoBeatGridNoLabel) {
    HoverReadoutCase c;
    c.pBeats = nullptr;
    c.pointerBeatDistance(17);
    const auto layout = c.layout();
    EXPECT_FALSE(layout.shown);
    EXPECT_TRUE(layout.text.isEmpty());
    // The time labels are unaffected.
    EXPECT_EQ("0:08", layoutTimeRuler(c.input()).distanceLabelText);
}

TEST(OverviewLayoutBeatDistanceTest, InactiveTimeRulerShowsNoLabel) {
    HoverReadoutCase c;
    c.state = TimeRulerState::Inactive;
    c.pointerBeatDistance(16);
    EXPECT_FALSE(c.layout().shown);
}

TEST(OverviewLayoutBeatDistanceTest, LeftDragAlsoShowsLabel) {
    HoverReadoutCase c;
    c.state = TimeRulerState::LeftDrag;
    c.pointerBeatDistance(16);
    EXPECT_TRUE(c.layout().shown);
    EXPECT_EQ("+4 bars", c.layout().text);
}

TEST(OverviewLayoutBeatDistanceTest, PointerIsClampedToOverview) {
    HoverReadoutCase c;
    c.playFrame = kTrackFrames / 2;
    c.pointerFrame = kTrackFrames * 2; // beyond the end
    c.settings.position = HoverReadoutPosition::AlwaysRight;
    EXPECT_EQ("+25 bars", c.layout().text);
}

TEST(OverviewLayoutBeatDistanceTest, UnloadedTrackShowsNoLabel) {
    HoverReadoutCase c;
    auto input = c.input();
    input.trackSamples = 0.0;
    EXPECT_FALSE(layoutHoverReadout(input).shown);
    input = c.input();
    input.overviewLength = 0.0;
    EXPECT_FALSE(layoutHoverReadout(input).shown);
}

// ---------------------------------------------------------------------------
// Plain hover readout
// ---------------------------------------------------------------------------

namespace {

HoverReadoutCase plainHover() {
    HoverReadoutCase c;
    c.state = TimeRulerState::Inactive;
    c.pointerBeatDistance(16);
    return c;
}

Input hoverInput(const HoverReadoutCase& c, bool pointerPresent = true) {
    Input in = c.input();
    in.pointerPresent = pointerPresent;
    return in;
}

} // namespace

TEST(OverviewLayoutHoverReadoutTest, PointerAbsentShowsNothing) {
    const auto c = plainHover();
    const auto in = hoverInput(c, false);
    EXPECT_FALSE(layoutHoverLine(in).lineShown);
    EXPECT_FALSE(layoutHoverReadout(in).shown);
}

TEST(OverviewLayoutHoverReadoutTest, LineAndLabelByDefault) {
    const auto c = plainHover();
    const auto in = hoverInput(c);
    const auto readout = layoutHoverLine(in);
    EXPECT_TRUE(readout.lineShown);
    EXPECT_DOUBLE_EQ(in.pointerPos, readout.linePos);
    const auto label = layoutHoverReadout(in);
    EXPECT_TRUE(label.shown);
    EXPECT_EQ("+4 bars", label.text);
    EXPECT_EQ(LabelSlot::OwnBottomRow, label.slot);
}

TEST(OverviewLayoutHoverReadoutTest, LineOnlyDisplay) {
    auto c = plainHover();
    c.settings.display = HoverDisplay::LineOnly;
    const auto in = hoverInput(c);
    EXPECT_TRUE(layoutHoverLine(in).lineShown);
    EXPECT_FALSE(layoutHoverReadout(in).shown);
}

TEST(OverviewLayoutHoverReadoutTest, LabelOnlyDisplay) {
    auto c = plainHover();
    c.settings.display = HoverDisplay::LabelOnly;
    const auto in = hoverInput(c);
    EXPECT_FALSE(layoutHoverLine(in).lineShown);
    EXPECT_TRUE(layoutHoverReadout(in).shown);
}

TEST(OverviewLayoutHoverReadoutTest, ShowHoverReadoutOffShowsNothingOnPlainHover) {
    auto c = plainHover();
    c.settings.showHoverReadout = false;
    const auto in = hoverInput(c);
    EXPECT_FALSE(layoutHoverLine(in).lineShown);
    EXPECT_FALSE(layoutHoverReadout(in).shown);
}

TEST(OverviewLayoutHoverReadoutTest, ShowHoverReadoutOffHidesReadoutOnTimeRuler) {
    for (auto state : {TimeRulerState::RightClickHold, TimeRulerState::LeftDrag}) {
        auto c = plainHover();
        c.state = state;
        c.settings.showHoverReadout = false;
        const auto in = hoverInput(c);
        EXPECT_FALSE(layoutHoverReadout(in).shown);
        // The time ruler itself is unchanged.
        EXPECT_TRUE(layoutTimeRuler(in).labelsShown);
        EXPECT_EQ(state == TimeRulerState::RightClickHold, layoutTimeRuler(in).lineShown);
    }
}

TEST(OverviewLayoutHoverReadoutTest, DisplayDoesNotAffectTimeRuler) {
    auto c = plainHover();
    c.state = TimeRulerState::RightClickHold;
    c.settings.display = HoverDisplay::LineOnly;
    EXPECT_TRUE(layoutHoverReadout(hoverInput(c)).shown);
}

TEST(OverviewLayoutHoverReadoutTest, NoBeatGridShowsLineOnly) {
    auto c = plainHover();
    c.pBeats = nullptr;
    const auto in = hoverInput(c);
    EXPECT_TRUE(layoutHoverLine(in).lineShown);
    EXPECT_FALSE(layoutHoverReadout(in).shown);
}

TEST(OverviewLayoutHoverReadoutTest, PlainHoverNeverShowsTimeLabels) {
    const auto c = plainHover();
    const auto in = hoverInput(c);
    EXPECT_FALSE(layoutTimeRuler(in).labelsShown);
    EXPECT_TRUE(layoutHoverReadout(in).joinedDistanceLabelText.isEmpty());
}

TEST(OverviewLayoutHoverReadoutTest, ShortOverviewKeepsOwnRowOnPlainHover) {
    auto c = plainHover();
    c.breadth = 30.0; // less than three label rows
    const auto label = layoutHoverReadout(hoverInput(c));
    EXPECT_EQ(LabelSlot::OwnBottomRow, label.slot);
    EXPECT_TRUE(label.joinedDistanceLabelText.isEmpty());
}

TEST(OverviewLayoutHoverReadoutTest, TimeRulerStatesDoNotUseHoverLine) {
    auto c = plainHover();
    c.state = TimeRulerState::LeftDrag;
    EXPECT_FALSE(layoutHoverLine(hoverInput(c)).lineShown);
}

// Hover mode: Extend time ruler (default) vs Separate hover line.
namespace {

const TimeRulerState kActiveStates[] = {
        TimeRulerState::RightClickHold, TimeRulerState::LeftDrag};

HoverReadoutCase hoverModeCase(HoverMode mode, TimeRulerState state) {
    auto c = plainHover();
    c.state = state;
    c.settings.hoverMode = mode;
    return c;
}

} // namespace

TEST(OverviewLayoutHoverModeTest, DefaultIsExtendTimeRuler) {
    EXPECT_EQ(HoverMode::ExtendTimeRuler, HoverReadoutSettings().hoverMode);
}

TEST(OverviewLayoutHoverModeTest, ExtendShowsReadoutOnTimeRuler) {
    for (auto state : kActiveStates) {
        const auto in = hoverInput(hoverModeCase(HoverMode::ExtendTimeRuler, state));
        EXPECT_TRUE(layoutHoverReadout(in).shown);
        EXPECT_FALSE(layoutHoverLine(in).lineShown);
    }
}

TEST(OverviewLayoutHoverModeTest, ExtendPlainHoverUnchanged) {
    const auto in = hoverInput(hoverModeCase(HoverMode::ExtendTimeRuler, TimeRulerState::Inactive));
    EXPECT_TRUE(layoutHoverReadout(in).shown);
    EXPECT_TRUE(layoutHoverLine(in).lineShown);
}

TEST(OverviewLayoutHoverModeTest, SeparatePlainHoverShowsLineAndLabel) {
    const auto in = hoverInput(
            hoverModeCase(HoverMode::SeparateHoverLine, TimeRulerState::Inactive));
    EXPECT_TRUE(layoutHoverReadout(in).shown);
    EXPECT_TRUE(layoutHoverLine(in).lineShown);
}

TEST(OverviewLayoutHoverModeTest, SeparatePlainHoverRespectsDisplay) {
    auto c = hoverModeCase(HoverMode::SeparateHoverLine, TimeRulerState::Inactive);
    c.settings.display = HoverDisplay::LineOnly;
    EXPECT_FALSE(layoutHoverReadout(hoverInput(c)).shown);
    EXPECT_TRUE(layoutHoverLine(hoverInput(c)).lineShown);
    c.settings.display = HoverDisplay::LabelOnly;
    EXPECT_TRUE(layoutHoverReadout(hoverInput(c)).shown);
    EXPECT_FALSE(layoutHoverLine(hoverInput(c)).lineShown);
}

TEST(OverviewLayoutHoverModeTest, SeparateHidesHoverReadoutWhileTimeRulerActive) {
    for (auto state : kActiveStates) {
        const auto in = hoverInput(hoverModeCase(HoverMode::SeparateHoverLine, state));
        EXPECT_FALSE(layoutHoverReadout(in).shown);
        EXPECT_FALSE(layoutHoverLine(in).lineShown);
    }
}

TEST(OverviewLayoutHoverModeTest, SeparateTimeRulerHasNoReadoutEvenOnShortOverview) {
    for (auto state : kActiveStates) {
        auto c = hoverModeCase(HoverMode::SeparateHoverLine, state);
        c.breadth = 30.0;
        const auto in = hoverInput(c);
        const auto label = layoutHoverReadout(in);
        EXPECT_FALSE(label.shown);
        EXPECT_TRUE(label.joinedDistanceLabelText.isEmpty());
        EXPECT_TRUE(layoutTimeRuler(in).labelsShown);
    }
}

TEST(OverviewLayoutHoverModeTest, SeparateTimeRulerIsUnchanged) {
    for (auto state : kActiveStates) {
        const auto sep = layoutTimeRuler(
                hoverInput(hoverModeCase(HoverMode::SeparateHoverLine, state)));
        const auto ext = layoutTimeRuler(
                hoverInput(hoverModeCase(HoverMode::ExtendTimeRuler, state)));
        EXPECT_EQ(ext.lineShown, sep.lineShown);
        EXPECT_EQ(ext.linePos, sep.linePos);
        EXPECT_EQ(ext.labelsShown, sep.labelsShown);
        EXPECT_EQ(ext.positionLabelText, sep.positionLabelText);
        EXPECT_EQ(ext.distanceLabelText, sep.distanceLabelText);
    }
}

TEST(OverviewLayoutHoverModeTest, SeparateHoverReadoutReturnsAfterTimeRulerEnds) {
    auto c = hoverModeCase(HoverMode::SeparateHoverLine, TimeRulerState::LeftDrag);
    EXPECT_FALSE(layoutHoverLine(hoverInput(c)).lineShown);
    c.state = TimeRulerState::Inactive;
    EXPECT_TRUE(layoutHoverLine(hoverInput(c)).lineShown);
    EXPECT_TRUE(layoutHoverReadout(hoverInput(c)).shown);
    // Pointer left the overview meanwhile: nothing comes back.
    EXPECT_FALSE(layoutHoverLine(hoverInput(c, false)).lineShown);
    EXPECT_FALSE(layoutHoverReadout(hoverInput(c, false)).shown);
}

TEST(OverviewLayoutHoverModeTest, ShowHoverReadoutOffShowsNothingInBothModes) {
    for (auto mode : {HoverMode::ExtendTimeRuler, HoverMode::SeparateHoverLine}) {
        for (auto state : {TimeRulerState::Inactive,
                     TimeRulerState::RightClickHold,
                     TimeRulerState::LeftDrag}) {
            auto c = hoverModeCase(mode, state);
            c.settings.showHoverReadout = false;
            const auto in = hoverInput(c);
            EXPECT_FALSE(layoutHoverReadout(in).shown);
            EXPECT_FALSE(layoutHoverLine(in).lineShown);
        }
    }
}

// Over a cue: hovering a cue (line or label) where Mixxx draws cue times.
namespace {

const HoverOverCue kAllOverCueChoices[] = {HoverOverCue::JoinCueTimes,
        HoverOverCue::HideHoverReadout,
        HoverOverCue::HideCueTimes,
        HoverOverCue::MoveHoverReadout};

// Plain hover with the pointer 2 bars (8 beats, 4 s) after the play position
// and a cue hovered, 4 bars before the play position. 1 px = 4800 frames =
// 0.2 beats at 120 BPM.
struct OverCueCase : HoverReadoutCase {
    double cueFrame = 400 + 100 * 24000.0 - 16 * 24000.0;
    bool cueTimesShown = true;
    bool cueHovered = true;
    // Extent of a hover readout label along the track axis: its width on a
    // horizontal overview, 10 px per character here.
    std::function<double(const QString&)> labelExtent = [](const QString& text) {
        return 10.0 * text.size();
    };

    explicit OverCueCase(HoverOverCue overCue = HoverOverCue::JoinCueTimes) {
        state = TimeRulerState::Inactive;
        settings.overCue = overCue;
        pointerBeatDistance(8);
    }
    double cuePos() const {
        return cueFrame / kTrackFrames * length;
    }
    Input cueInput() const {
        Input in = input();
        in.pointerPresent = true;
        in.cueHovered = cueHovered;
        in.hoveredCueSamples = cueFrame * 2;
        in.hoveredCuePos = cuePos();
        in.cueTimesShown = cueTimesShown;
        in.labelGap = 1.0;
        in.labelExtent = labelExtent;
        return in;
    }
    HoverReadoutLayout readout() const {
        return layoutHoverReadout(cueInput());
    }
};

} // namespace

TEST(OverviewLayoutOverCueTest, JoinAppendsBeatDistanceToCueTimeDistance) {
    const OverCueCase c;
    const auto readout = c.readout();
    EXPECT_TRUE(readout.shown);
    EXPECT_EQ(LabelSlot::JoinedToCueTimeDistanceLabel, readout.slot);
    EXPECT_EQ(LabelSide::Right, readout.side);
    EXPECT_EQ("-4 bars", readout.text);
    // 4 bars at 120 BPM = 8 s before the play position
    EXPECT_EQ("-0:08  -4 bars", readout.joinedDistanceLabelText);
    EXPECT_FALSE(layoutCueTimes(c.cueInput()).hidden);
}

TEST(OverviewLayoutOverCueTest, JoinedTextAheadOfAndAtThePlayPosition) {
    OverCueCase c;
    c.cueFrame = c.playFrame + 33 * 24000.0; // 16.5 s, 8.25 bars
    c.settings.precision = BeatDistancePrecision::Fractional;
    EXPECT_EQ("0:16  +8.25 bars", c.readout().joinedDistanceLabelText);
    c.settings.unit = BeatDistanceUnit::Beats;
    EXPECT_EQ("0:16  +33.0 beats", c.readout().joinedDistanceLabelText);
    c.cueFrame = c.playFrame;
    c.settings.unit = BeatDistanceUnit::Bars;
    c.settings.precision = BeatDistancePrecision::Whole;
    EXPECT_EQ("0:00  0 bars", c.readout().joinedDistanceLabelText);
}

TEST(OverviewLayoutOverCueTest, JoinIgnoresLabelPosition) {
    OverCueCase c;
    c.settings.position = HoverReadoutPosition::AlwaysLeft;
    const auto readout = c.readout();
    EXPECT_EQ(LabelSide::Right, readout.side);
    EXPECT_EQ("-0:08  -4 bars", readout.joinedDistanceLabelText);
}

TEST(OverviewLayoutOverCueTest, BeatDistanceIsMeasuredToTheCueNotThePointer) {
    // The pointer is on the cue label, 12 bars after the cue line.
    for (const auto choice : {HoverOverCue::HideCueTimes, HoverOverCue::MoveHoverReadout}) {
        OverCueCase c(choice);
        c.pointerBeatDistance(32);
        const auto readout = c.readout();
        EXPECT_TRUE(readout.shown);
        EXPECT_TRUE(readout.text.contains("-4")) << readout.text.toStdString();
    }
    OverCueCase c;
    c.pointerBeatDistance(32);
    EXPECT_EQ("-0:08  -4 bars", c.readout().joinedDistanceLabelText);
}

TEST(OverviewLayoutOverCueTest, NoCueNoChange) {
    for (const auto choice : kAllOverCueChoices) {
        OverCueCase c(choice);
        c.cueHovered = false;
        const auto in = c.cueInput();
        const auto readout = layoutHoverReadout(in);
        EXPECT_TRUE(readout.shown);
        EXPECT_EQ("+2 bars", readout.text);
        EXPECT_EQ(LabelSlot::OwnBottomRow, readout.slot);
        EXPECT_EQ(LabelSide::Right, readout.side);
        EXPECT_DOUBLE_EQ(in.pointerPos, readout.anchorPos);
        EXPECT_TRUE(layoutHoverLine(in).lineShown);
        EXPECT_FALSE(layoutCueTimes(in).hidden);
    }
}

TEST(OverviewLayoutOverCueTest, HoverLineIsHiddenOverACueForEveryDisplay) {
    for (const auto display : {HoverDisplay::LineAndLabel,
                 HoverDisplay::LineOnly,
                 HoverDisplay::LabelOnly}) {
        for (const auto choice : kAllOverCueChoices) {
            OverCueCase c(choice);
            c.settings.display = display;
            EXPECT_FALSE(layoutHoverLine(c.cueInput()).lineShown);
        }
    }
}

TEST(OverviewLayoutOverCueTest, HideHoverReadoutDrawsNothingAndKeepsCueTimes) {
    const OverCueCase c(HoverOverCue::HideHoverReadout);
    EXPECT_FALSE(c.readout().shown);
    EXPECT_FALSE(layoutCueTimes(c.cueInput()).hidden);
}

TEST(OverviewLayoutOverCueTest, HideCueTimesDrawsReadoutInOwnRowAtTheCue) {
    const OverCueCase c(HoverOverCue::HideCueTimes);
    const auto readout = c.readout();
    EXPECT_TRUE(readout.shown);
    EXPECT_EQ(LabelSlot::OwnBottomRow, readout.slot);
    EXPECT_EQ(LabelSide::Right, readout.side);
    EXPECT_EQ("-4 bars", readout.text);
    EXPECT_DOUBLE_EQ(c.cuePos(), readout.anchorPos);
    EXPECT_TRUE(layoutCueTimes(c.cueInput()).hidden);
}

TEST(OverviewLayoutOverCueTest, HideCueTimesFollowsLabelPosition) {
    OverCueCase c(HoverOverCue::HideCueTimes);
    c.settings.position = HoverReadoutPosition::AlwaysLeft;
    const auto readout = c.readout();
    EXPECT_EQ(LabelSide::Left, readout.side);
    EXPECT_EQ("bars -4", readout.text);
}

TEST(OverviewLayoutOverCueTest, MoveDrawsReadoutLeftOfTheCueIgnoringLabelPosition) {
    OverCueCase c(HoverOverCue::MoveHoverReadout);
    c.settings.position = HoverReadoutPosition::AlwaysRight;
    const auto readout = c.readout();
    EXPECT_TRUE(readout.shown);
    EXPECT_EQ(LabelSlot::OwnBottomRow, readout.slot);
    EXPECT_EQ(LabelSide::Left, readout.side);
    EXPECT_EQ("bars -4", readout.text);
    EXPECT_DOUBLE_EQ(c.cuePos(), readout.anchorPos);
    EXPECT_FALSE(layoutCueTimes(c.cueInput()).hidden);
}

TEST(OverviewLayoutOverCueTest, MoveIsNotDrawnWhereItDoesNotFitBeforeTheStartEdge) {
    OverCueCase c(HoverOverCue::MoveHoverReadout);
    // Near the start edge the cue is ~21 bars before the play position:
    // "bars -21" is 8 chars = 80 px wide in this test, plus the 1 px gap.
    c.cueFrame = 81 * 4800.0;
    EXPECT_DOUBLE_EQ(81.0, c.cuePos());
    EXPECT_EQ("bars -21", c.readout().text);
    EXPECT_TRUE(c.readout().shown); // 81 - 1 - 80 = 0: just fits
    c.cueFrame = 80.9 * 4800.0;
    EXPECT_FALSE(c.readout().shown);
    c.cueFrame = 5 * 4800.0;
    EXPECT_FALSE(c.readout().shown);
    // The cue times are still Mixxx's.
    EXPECT_FALSE(layoutCueTimes(c.cueInput()).hidden);
}

TEST(OverviewLayoutOverCueTest, MoveFitUsesTheLabelExtentAlongTheTrackAxis) {
    OverCueCase c(HoverOverCue::MoveHoverReadout);
    // Like a vertical overview: the label extends above the line by its
    // height (15 px), not by its width (80 px).
    c.labelExtent = [](const QString&) { return 15.0; };
    c.cueFrame = 20 * 4800.0;
    EXPECT_DOUBLE_EQ(20.0, c.cuePos());
    EXPECT_TRUE(c.readout().shown); // 20 - 1 - 15 >= 0
    c.cueFrame = 15 * 4800.0;
    EXPECT_FALSE(c.readout().shown); // 15 - 1 - 15 < 0
}

TEST(OverviewLayoutOverCueTest, CueTimesAreUpstreamsUnlessJoined) {
    for (const auto choice : {HoverOverCue::HideHoverReadout, HoverOverCue::MoveHoverReadout}) {
        OverCueCase c(choice);
        c.cueFrame = 40 * 48000.0; // 40 s into the track
        c.playFrame = 50 * 48000.0;
        const auto cueTimes = layoutCueTimes(c.cueInput());
        EXPECT_FALSE(cueTimes.hidden);
        EXPECT_EQ("0:40 -1:00", cueTimes.positionLabelText);
        EXPECT_EQ("-0:10", cueTimes.distanceLabelText);
    }
}

TEST(OverviewLayoutOverCueTest, JoinedCueTimesDistanceLabelCarriesTheBeatDistance) {
    const OverCueCase c;
    const auto cueTimes = layoutCueTimes(c.cueInput());
    EXPECT_FALSE(cueTimes.hidden);
    EXPECT_EQ("-0:08  -4 bars", cueTimes.distanceLabelText);
    EXPECT_FALSE(cueTimes.positionLabelText.isEmpty());
}

TEST(OverviewLayoutOverCueTest, HiddenCueTimesHaveNoText) {
    const OverCueCase c(HoverOverCue::HideCueTimes);
    const auto cueTimes = layoutCueTimes(c.cueInput());
    EXPECT_TRUE(cueTimes.hidden);
    EXPECT_TRUE(cueTimes.positionLabelText.isEmpty());
    EXPECT_TRUE(cueTimes.distanceLabelText.isEmpty());
}

TEST(OverviewLayoutOverCueTest, ACueThatIsNotRenderedGivesTheNoCueLayout) {
    // The hovered cue stopped being rendered (deleted, cleared from a
    // controller): the overview says no cue is hovered, whatever position
    // the stale cue had.
    for (const auto choice : kAllOverCueChoices) {
        OverCueCase c(choice);
        c.cueHovered = false;
        const auto cueTimes = layoutCueTimes(c.cueInput());
        EXPECT_FALSE(cueTimes.hidden);
        EXPECT_TRUE(cueTimes.positionLabelText.isEmpty());
        EXPECT_TRUE(cueTimes.distanceLabelText.isEmpty());
        EXPECT_NE(LabelSlot::JoinedToCueTimeDistanceLabel, c.readout().slot);
        EXPECT_TRUE(layoutHoverLine(c.cueInput()).lineShown);
    }
}

TEST(OverviewLayoutOverCueTest, SettingHasNoEffectWhenSkinHidesCueTimes) {
    for (const auto choice : kAllOverCueChoices) {
        OverCueCase c(choice);
        c.cueTimesShown = false;
        const auto readout = c.readout();
        EXPECT_TRUE(readout.shown);
        EXPECT_EQ(LabelSlot::OwnBottomRow, readout.slot);
        EXPECT_EQ(LabelSide::Right, readout.side);
        EXPECT_EQ("-4 bars", readout.text);
        EXPECT_DOUBLE_EQ(c.cuePos(), readout.anchorPos);
        EXPECT_FALSE(layoutCueTimes(c.cueInput()).hidden);
        // The cue is still the target.
        EXPECT_FALSE(layoutHoverLine(c.cueInput()).lineShown);
    }
}

TEST(OverviewLayoutOverCueTest, SettingHasNoEffectWithoutAReadoutLabel) {
    for (const auto choice : {HoverOverCue::JoinCueTimes, HoverOverCue::HideCueTimes}) {
        {
            OverCueCase c(choice);
            c.pBeats = nullptr;
            EXPECT_FALSE(c.readout().shown);
            EXPECT_FALSE(layoutCueTimes(c.cueInput()).hidden);
        }
        {
            OverCueCase c(choice);
            c.settings.showHoverReadout = false;
            EXPECT_FALSE(c.readout().shown);
            EXPECT_FALSE(layoutCueTimes(c.cueInput()).hidden);
        }
        {
            OverCueCase c(choice);
            c.settings.display = HoverDisplay::LineOnly;
            EXPECT_FALSE(c.readout().shown);
            EXPECT_FALSE(layoutCueTimes(c.cueInput()).hidden);
        }
    }
}

TEST(OverviewLayoutOverCueTest, LabelOnlyDisplayStillTargetsTheCue) {
    OverCueCase c(HoverOverCue::HideCueTimes);
    c.settings.display = HoverDisplay::LabelOnly;
    EXPECT_EQ("-4 bars", c.readout().text);
    EXPECT_TRUE(layoutCueTimes(c.cueInput()).hidden);
}

// ---------------------------------------------------------------------------
// Snapped seek
// ---------------------------------------------------------------------------

namespace {

// Frame of a (fractional) beat index on kConstTempoBeats.
double constBeatFrame(double beatIndex) {
    return 400 + beatIndex * 24000.0;
}

// Overview pixel of a frame (1000 px for kTrackFrames).
double framePx(double frame) {
    return frame / kTrackFrames * kOverviewLength;
}

struct SnapCase : HoverReadoutCase {
    SnapCase() {
        settings.snapSeeks = true;
        state = TimeRulerState::Inactive;
        settings.unit = BeatDistanceUnit::Bars;
        settings.precision = BeatDistancePrecision::Whole;
        playBeat(100);
        pointerBeat(100);
    }
    SnapCase& playBeat(double beatIndex) {
        playFrame = constBeatFrame(beatIndex);
        return *this;
    }
    SnapCase& pointerBeat(double beatIndex) {
        pointerFrame = constBeatFrame(beatIndex);
        return *this;
    }
    Input snapInput() const {
        Input in = input();
        in.pointerPresent = true;
        return in;
    }
    SnappedSeekLayout snap() const {
        return layoutSnappedSeek(snapInput());
    }
};

} // namespace

TEST(OverviewLayoutSnappedSeekTest, SnapsToTheBarLineNearestThePointer) {
    // Bar lines are on beats 104, 108, 112. The midpoint between 108 and 112
    // is beat 110.
    SnapCase c;
    c.pointerBeat(109.9);
    auto snap = c.snap();
    EXPECT_TRUE(snap.snapped);
    EXPECT_NEAR(framePx(constBeatFrame(108)), snap.linePos, 1e-9);
    EXPECT_EQ(2, snap.jumpCount);

    c.pointerBeat(110.1);
    snap = c.snap();
    EXPECT_NEAR(framePx(constBeatFrame(112)), snap.linePos, 1e-9);
    EXPECT_EQ(3, snap.jumpCount);
}

TEST(OverviewLayoutSnappedSeekTest, HoverLineFollowsTheSnappedLine) {
    SnapCase c;
    c.pointerBeat(109.9);
    const auto line = layoutHoverLine(c.snapInput());
    EXPECT_TRUE(line.lineShown);
    EXPECT_NEAR(framePx(constBeatFrame(108)), line.linePos, 1e-9);
}

TEST(OverviewLayoutSnappedSeekTest, HoverLineStaysPutWhilePlayMarkerCrossesBarLines) {
    // Still mouse on a playing deck: only the count changes. The play marker
    // passes the bar line on beat 100 (bar 25).
    SnapCase c;
    c.pointerBeat(163.9);
    c.playBeat(99.9);
    auto snap = c.snap();
    const double linePos = snap.linePos;
    EXPECT_EQ(17, snap.jumpCount); // bar 24 -> bar 41
    c.playBeat(100.1);
    snap = c.snap();
    EXPECT_DOUBLE_EQ(linePos, snap.linePos);
    EXPECT_EQ(16, snap.jumpCount); // bar 25 -> bar 41
}

TEST(OverviewLayoutSnappedSeekTest, JumpCountWithPlayMarkerOffItsBarLine) {
    // The play marker is 0.3 beats after the line of the bar starting on beat
    // 100; the pointer is nearest the line on beat 164, 16 bars later.
    SnapCase c;
    c.playBeat(100.3);
    c.pointerBeat(164.6);
    const auto snap = c.snap();
    EXPECT_EQ(16, snap.jumpCount);
    EXPECT_EQ(64, snap.jumpBeats);
    EXPECT_NEAR(framePx(constBeatFrame(164)), snap.linePos, 1e-9);
    // Exactly 64 beats later along the grid, keeping the offset.
    EXPECT_NEAR(2 * constBeatFrame(164.3), snap.landingSamples, 1e-6);
}

TEST(OverviewLayoutSnappedSeekTest, PlayMarkerOnItsBarLineLandsOnTheTargetBarLine) {
    SnapCase c;
    c.pointerBeat(163.9);
    const auto snap = c.snap();
    EXPECT_EQ(16, snap.jumpCount);
    EXPECT_NEAR(2 * constBeatFrame(164), snap.landingSamples, 1e-6);
}

TEST(OverviewLayoutSnappedSeekTest, BackwardJump) {
    SnapCase c;
    c.playBeat(100.3);
    c.pointerBeat(36.2);
    const auto snap = c.snap();
    EXPECT_EQ(-16, snap.jumpCount);
    EXPECT_NEAR(2 * constBeatFrame(36.3), snap.landingSamples, 1e-6);
}

TEST(OverviewLayoutSnappedSeekTest, SnappingToThePlayMarkersOwnBarDoesNothing) {
    SnapCase c;
    c.playBeat(101.5);
    c.pointerBeat(100.5);
    auto snap = c.snap();
    EXPECT_TRUE(snap.snapped);
    EXPECT_EQ(0, snap.jumpCount);
    EXPECT_EQ(0, snap.jumpBeats);
    EXPECT_NEAR(framePx(constBeatFrame(100)), snap.linePos, 1e-9);
    EXPECT_NEAR(2 * constBeatFrame(101.5), snap.landingSamples, 1e-6);
    EXPECT_EQ("0 bars", layoutHoverReadout(c.snapInput()).text);

    // Past the middle of the own bar the next bar's line is nearest.
    c.pointerBeat(102.5);
    snap = c.snap();
    EXPECT_EQ(1, snap.jumpCount);
}

TEST(OverviewLayoutSnappedSeekTest, BeatsUnitSnapsToBeats) {
    SnapCase c;
    c.settings.unit = BeatDistanceUnit::Beats;
    c.playBeat(100.3);
    c.pointerBeat(105.6);
    auto snap = c.snap();
    EXPECT_EQ(6, snap.jumpCount); // beat 106 - beat 100
    EXPECT_EQ(6, snap.jumpBeats);
    EXPECT_NEAR(framePx(constBeatFrame(106)), snap.linePos, 1e-9);
    EXPECT_NEAR(2 * constBeatFrame(106.3), snap.landingSamples, 1e-6);
    EXPECT_EQ("+6 beats", layoutHoverReadout(c.snapInput()).text);

    c.pointerBeat(105.4);
    EXPECT_EQ(5, c.snap().jumpCount);
}

TEST(OverviewLayoutSnappedSeekTest, SnappingNeverUsesDecimals) {
    SnapCase c;
    c.settings.precision = BeatDistancePrecision::Fractional;
    c.pointerBeat(109.9);
    const auto snap = c.snap();
    EXPECT_EQ(2, snap.jumpCount);
    EXPECT_NEAR(framePx(constBeatFrame(108)), snap.linePos, 1e-9);
}

TEST(OverviewLayoutSnappedSeekTest, WholeLabelShowsTheJumpCount) {
    SnapCase c;
    c.settings.position = HoverReadoutPosition::AlwaysRight;
    c.playBeat(100.3);
    c.pointerBeat(164.6);
    const auto readout = layoutHoverReadout(c.snapInput());
    EXPECT_TRUE(readout.shown);
    EXPECT_EQ("+16 bars", readout.text);
    // The label sits at the snapped line.
    EXPECT_NEAR(framePx(constBeatFrame(164)), readout.anchorPos, 1e-9);
}

TEST(OverviewLayoutSnappedSeekTest, FractionalLabelShowsTheTrueBeatDistance) {
    // The play marker is 0.075 bar (0.3 beats) into bar 10 (beat 40), the
    // pointer is at 25.525 bars: the label shows 15.45 bars while the jump
    // is to bar 26, 16 bars.
    SnapCase c;
    c.settings.precision = BeatDistancePrecision::Fractional;
    c.playBeat(40.3);
    c.pointerBeat(25.525 * 4);
    EXPECT_EQ("+15.45 bars", layoutHoverReadout(c.snapInput()).text);
    EXPECT_EQ(16, c.snap().jumpCount);

    c.settings.precision = BeatDistancePrecision::Whole;
    EXPECT_EQ("+16 bars", layoutHoverReadout(c.snapInput()).text);
}

TEST(OverviewLayoutSnappedSeekTest, LabelIsNotNeededForSnapping) {
    SnapCase c;
    c.pointerBeat(163.9);
    c.settings.showHoverReadout = false;
    EXPECT_EQ(16, c.snap().jumpCount);
    c.settings.showHoverReadout = true;
    c.settings.display = HoverDisplay::LineOnly;
    EXPECT_EQ(16, c.snap().jumpCount);
    c.settings.hoverMode = HoverMode::SeparateHoverLine;
    c.state = TimeRulerState::LeftDrag;
    EXPECT_EQ(16, c.snap().jumpCount);
}

TEST(OverviewLayoutSnappedSeekTest, TimeRulerLineSnapsInBothHoverModes) {
    for (const auto mode : {HoverMode::ExtendTimeRuler, HoverMode::SeparateHoverLine}) {
        SnapCase c;
        c.settings.hoverMode = mode;
        c.state = TimeRulerState::RightClickHold;
        c.pointerBeat(109.9);
        const auto ruler = layoutTimeRuler(c.snapInput());
        EXPECT_TRUE(ruler.lineShown);
        EXPECT_NEAR(framePx(constBeatFrame(108)), ruler.linePos, 1e-9);
    }
}

TEST(OverviewLayoutSnappedSeekTest, TimeRulerLabelsDescribeTheLandingSpot) {
    // 120 BPM from frame 0: one beat is 0.5 s. The play marker is on beat
    // 100.5 (50.25 s); the pointer is nearest the bar line on beat 164, so
    // the landing spot is beat 164.5 (82.25 s of the 100 s track).
    const auto beats = Beats(audio::FramePos(0), kBpm, kSampleRate, QString());
    SnapCase c;
    c.pBeats = &beats;
    c.playFrame = 100.5 * 24000;
    c.pointerFrame = 163.5 * 24000;
    c.state = TimeRulerState::RightClickHold;
    const auto ruler = layoutTimeRuler(c.snapInput());
    EXPECT_EQ("1:22 -0:17", ruler.positionLabelText);
    EXPECT_EQ("0:32", ruler.distanceLabelText);
    EXPECT_NEAR(framePx(164 * 24000.0), ruler.linePos, 1e-9);

    // Left-drag has the same labels (and no ruler line).
    c.state = TimeRulerState::LeftDrag;
    const auto drag = layoutTimeRuler(c.snapInput());
    EXPECT_FALSE(drag.lineShown);
    EXPECT_EQ(ruler.positionLabelText, drag.positionLabelText);
    EXPECT_EQ(ruler.distanceLabelText, drag.distanceLabelText);
}

TEST(OverviewLayoutSnappedSeekTest, JoinedDistanceLabelUsesTheLandingSpot) {
    const auto beats = Beats(audio::FramePos(0), kBpm, kSampleRate, QString());
    SnapCase c;
    c.pBeats = &beats;
    c.playFrame = 100.5 * 24000;
    c.pointerFrame = 163.5 * 24000;
    c.state = TimeRulerState::RightClickHold;
    c.breadth = 20.0; // too short for three label rows
    const auto readout = layoutHoverReadout(c.snapInput());
    EXPECT_EQ("0:32  +16 bars", readout.joinedDistanceLabelText);
}

TEST(OverviewLayoutSnappedSeekTest, BarLinesContinueBeforeTheFirstBeat) {
    // The first beat is 5 beats into the track, so the bar lines before it
    // are on beats -4 (frame 24000) and -8 (before the start).
    const auto beats = Beats(audio::FramePos(5 * 24000), kBpm, kSampleRate, QString());
    SnapCase c;
    c.pBeats = &beats;
    c.playFrame = (5 + 6) * 24000.0; // beat index 6: bar 1, 2 beats in
    c.pointerFrame = 30000;          // beat index -3.75
    const auto snap = c.snap();
    EXPECT_EQ(-2, snap.jumpCount);
    EXPECT_NEAR(framePx(24000), snap.linePos, 1e-9);
    EXPECT_NEAR(2 * (5 + 6 - 8) * 24000.0, snap.landingSamples, 1e-6);
}

TEST(OverviewLayoutSnappedSeekTest, LandingPastTheEndIsClampedToTheEnd) {
    SnapCase c;
    c.playBeat(100.3);
    c.pointerFrame = kTrackFrames; // nearest bar line is beat 200, 25 bars on
    const auto snap = c.snap();
    EXPECT_EQ(25, snap.jumpCount);
    EXPECT_DOUBLE_EQ(kTrackFrames * 2, snap.landingSamples);

    c.state = TimeRulerState::RightClickHold;
    EXPECT_EQ("1:40 -0:00", layoutTimeRuler(c.snapInput()).positionLabelText);
}

TEST(OverviewLayoutSnappedSeekTest, VariableTempoGridKeepsTheOffsetAlongTheGrid) {
    // Beats 0-8 are 24000 frames long, 8-24 are 48000 frames long. The play
    // marker is on beat 6.5; the pointer is on the line of bar 3 (beat 12).
    // 2 bars (8 beats) later is beat 14.5: half a (long) beat after beat 14.
    SnapCase c;
    c.pBeats = &kNonConstTempoBeats;
    c.playFrame = 400 + 6.5 * 24000;
    c.pointerFrame = 400 + 192000 + 4 * 48000.0;
    const auto snap = c.snap();
    EXPECT_EQ(2, snap.jumpCount);
    EXPECT_EQ(8, snap.jumpBeats);
    EXPECT_NEAR(framePx(400 + 192000 + 4 * 48000.0), snap.linePos, 1e-9);
    EXPECT_NEAR(2 * 504400.0, snap.landingSamples, 1e-6);
}

TEST(OverviewLayoutSnappedSeekTest, SettingOffBehavesAsBefore) {
    SnapCase c;
    c.settings.snapSeeks = false;
    c.settings.position = HoverReadoutPosition::AlwaysRight;
    c.playBeat(100.3);
    c.pointerBeat(164.6);
    const auto snap = c.snap();
    EXPECT_FALSE(snap.snapped);
    EXPECT_EQ(0, snap.jumpBeats);
    EXPECT_NEAR(framePx(constBeatFrame(164.6)), snap.linePos, 1e-9);
    EXPECT_NEAR(2 * constBeatFrame(164.6), snap.landingSamples, 1e-6);
    // Line and label follow the pointer.
    EXPECT_NEAR(framePx(constBeatFrame(164.6)), layoutHoverLine(c.snapInput()).linePos, 1e-9);
    EXPECT_EQ("+16 bars", layoutHoverReadout(c.snapInput()).text); // 16.075 bars
    c.pointerBeat(160.9);
    EXPECT_EQ("+15 bars", layoutHoverReadout(c.snapInput()).text); // 15.15 bars
    c.state = TimeRulerState::RightClickHold;
    EXPECT_NEAR(framePx(constBeatFrame(160.9)), layoutTimeRuler(c.snapInput()).linePos, 1e-9);
}

TEST(OverviewLayoutSnappedSeekTest, NoBeatGridBehavesAsBefore) {
    SnapCase c;
    c.pBeats = nullptr;
    c.pointerBeat(164.6);
    const auto snap = c.snap();
    EXPECT_FALSE(snap.snapped);
    EXPECT_EQ(0, snap.jumpBeats);
    EXPECT_NEAR(framePx(constBeatFrame(164.6)), snap.linePos, 1e-9);
    EXPECT_NEAR(2 * constBeatFrame(164.6), snap.landingSamples, 1e-6);
    EXPECT_NEAR(framePx(constBeatFrame(164.6)), layoutHoverLine(c.snapInput()).linePos, 1e-9);
    EXPECT_FALSE(layoutHoverReadout(c.snapInput()).shown);
}

TEST(OverviewLayoutSnappedSeekTest, HoveredCueIsStillTheTargetOfTheReadout) {
    SnapCase c;
    c.playBeat(100.3);
    Input in = c.snapInput();
    in.cueHovered = true;
    in.hoveredCueSamples = 2 * constBeatFrame(90.5);
    in.hoveredCuePos = framePx(constBeatFrame(90.5));
    const auto readout = layoutHoverReadout(in);
    EXPECT_NEAR(in.hoveredCuePos, readout.anchorPos, 1e-9);
    EXPECT_FALSE(layoutHoverLine(in).lineShown);
}
