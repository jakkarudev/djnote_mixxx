#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "audio/frame.h"
#include "audio/types.h"
#include "track/beats.h"
#include "track/bpm.h"
#include "waveform/scrollinglinelayout.h"

namespace {

using namespace mixxx;
using namespace mixxx::scrollinglinelayout;

constexpr auto kBpm = Bpm(120.0);
constexpr auto kSampleRate = audio::SampleRate(48000);
// A beat at 120 BPM / 48 kHz is 24000 frames; a bar is 96000 frames.
constexpr double kBeatFrames = 24000.0;

// A made-up screen mapping: 1 px per 100 frames, shifted by 7 px.
double px(double frame) {
    return 7.0 + frame / 100.0;
}

const std::function<double(audio::FramePos)> kFrameToX = [](audio::FramePos pos) {
    return px(pos.value());
};

Beats constBeats(double startFrame) {
    return Beats(audio::FramePos(startFrame), kBpm, kSampleRate, QString());
}

// 8 beats at 120 BPM, then 16 beats at 60 BPM, then 120 BPM.
const Beats kTempoChangeBeats(
        std::vector<BeatMarker>{
                BeatMarker{audio::FramePos(400), 8},
                BeatMarker{audio::FramePos(400 + 8 * 24000), 16},
        },
        audio::FramePos(400 + 8 * 24000 + 16 * 48000),
        kBpm,
        kSampleRate,
        QString());

// The skin's beat colour passed in by the renderer.
const QColor kSkinColor(10, 20, 30);

std::vector<Line> lines(const Beats& beats,
        double startFrame,
        double endFrame,
        const LineSettings& settings = LineSettings(),
        int upstreamOpacity = 90) {
    return layoutLines(&beats,
            audio::FramePos(startFrame),
            audio::FramePos(endFrame),
            kFrameToX,
            settings,
            upstreamOpacity,
            kSkinColor);
}

// Checks positions and kinds; 'B' = bar line, 'b' = beat line, 's' = step line.
void expectLines(const std::vector<double>& expectedFrames,
        const std::string& expectedKinds,
        const std::vector<Line>& actual) {
    ASSERT_EQ(expectedFrames.size(), actual.size());
    ASSERT_EQ(expectedKinds.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i) {
        EXPECT_NEAR(px(expectedFrames[i]), actual[i].x, 1e-6) << "index " << i;
        const LineKind kind = expectedKinds[i] == 'B'
                ? LineKind::Bar
                : (expectedKinds[i] == 's' ? LineKind::Step : LineKind::Beat);
        EXPECT_EQ(kind, actual[i].kind) << "index " << i;
    }
}

// Default settings with step lines shown.
LineSettings stepSettings() {
    LineSettings settings;
    settings.stepsEnabled = true;
    return settings;
}

} // namespace

TEST(ScrollingLineLayoutTest, BarLineOnEveryFourthBeatFromTheFirstBeat) {
    const Beats beats = constBeats(400);
    // Beats 0..5 of the grid.
    const auto result = lines(beats, 0, 400 + 5 * kBeatFrames);
    expectLines({400,
                        400 + 1 * kBeatFrames,
                        400 + 2 * kBeatFrames,
                        400 + 3 * kBeatFrames,
                        400 + 4 * kBeatFrames,
                        400 + 5 * kBeatFrames},
            "BbbbBb",
            result);
}

TEST(ScrollingLineLayoutTest, BarsCountFromTheFirstBeatWhenItIsOffScreen) {
    const Beats beats = constBeats(400);
    // Beats 6..9 of the grid: beat 8 is a bar line.
    const auto result = lines(beats, 400 + 5.5 * kBeatFrames, 400 + 9.5 * kBeatFrames);
    expectLines({400 + 6 * kBeatFrames,
                        400 + 7 * kBeatFrames,
                        400 + 8 * kBeatFrames,
                        400 + 9 * kBeatFrames},
            "bbBb",
            result);
}

TEST(ScrollingLineLayoutTest, BarsContinueBackThroughTheIntro) {
    // The grid's first beat is at 400 + 4 beats. Bars are counted back from
    // it through the intro (ADR 0001): the beat four before it is a bar line.
    const Beats beats = constBeats(400 + 4 * kBeatFrames);
    const auto result = lines(beats, 0, 400 + 4 * kBeatFrames);
    expectLines({400,
                        400 + 1 * kBeatFrames,
                        400 + 2 * kBeatFrames,
                        400 + 3 * kBeatFrames,
                        400 + 4 * kBeatFrames},
            "BbbbB",
            result);
}

TEST(ScrollingLineLayoutTest, TempoChangeGrid) {
    // Beats 6..12: 6 and 7 at 120 BPM, 8.. at 60 BPM (48000 frames apart).
    const double tempoChange = 400 + 8 * kBeatFrames;
    const auto result = lines(kTempoChangeBeats, 400 + 6 * kBeatFrames, tempoChange + 4 * 48000);
    expectLines({400 + 6 * kBeatFrames,
                        400 + 7 * kBeatFrames,
                        tempoChange,
                        tempoChange + 1 * 48000,
                        tempoChange + 2 * 48000,
                        tempoChange + 3 * 48000,
                        tempoChange + 4 * 48000},
            "bbBbbbB",
            result);
}

TEST(ScrollingLineLayoutTest, DefaultOpacitiesAreBar100Beat80) {
    const Beats beats = constBeats(400);
    const auto result = lines(beats, 0, 400 + 1 * kBeatFrames);
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ(100, result[0].opacityPercent);
    EXPECT_EQ(80, result[1].opacityPercent);
}

TEST(ScrollingLineLayoutTest, OpacitiesAreClamped) {
    const Beats beats = constBeats(400);
    LineSettings settings;
    settings.barOpacity = OpacityPercent(250);
    settings.beatOpacity = OpacityPercent(55);
    const auto result = lines(beats, 0, 400 + 1 * kBeatFrames, settings);
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ(100, result[0].opacityPercent);
    EXPECT_EQ(55, result[1].opacityPercent);

    settings.enabled = false;
    const auto upstream = lines(beats, 0, 400 + 1 * kBeatFrames, settings, 180);
    ASSERT_EQ(2u, upstream.size());
    EXPECT_EQ(100, upstream[0].opacityPercent);
    EXPECT_EQ(100, upstream[1].opacityPercent);
}

TEST(ScrollingLineLayoutTest, LinesAtZeroOpacityAreNotReturned) {
    const Beats beats = constBeats(400);
    LineSettings settings;
    settings.beatOpacity = OpacityPercent(-10);
    const auto result = lines(beats, 0, 400 + 5 * kBeatFrames, settings);
    expectLines({400, 400 + 4 * kBeatFrames}, "BB", result);

    settings.enabled = false;
    EXPECT_TRUE(lines(beats, 0, 400 + 5 * kBeatFrames, settings, 0).empty());
}

TEST(ScrollingLineLayoutTest, MasterSwitchOffReproducesUpstream) {
    // Upstream draws every beat in range alike at its beat grid opacity.
    LineSettings settings;
    settings.enabled = false;
    const double tempoChange = 400 + 8 * kBeatFrames;
    const auto result = lines(kTempoChangeBeats,
            400 + 6 * kBeatFrames,
            tempoChange + 4 * 48000,
            settings,
            90);
    expectLines({400 + 6 * kBeatFrames,
                        400 + 7 * kBeatFrames,
                        tempoChange,
                        tempoChange + 1 * 48000,
                        tempoChange + 2 * 48000,
                        tempoChange + 3 * 48000,
                        tempoChange + 4 * 48000},
            "bbbbbbb",
            result);
    for (const auto& line : result) {
        EXPECT_EQ(90, line.opacityPercent);
    }
}

TEST(ScrollingLineLayoutTest, EmptyAndEdgeRanges) {
    const Beats beats = constBeats(400);
    // No beat grid.
    EXPECT_TRUE(layoutLines(nullptr,
            audio::FramePos(0),
            audio::FramePos(100000),
            kFrameToX,
            LineSettings(),
            90,
            kSkinColor)
                        .empty());
    // Invalid or reversed range.
    EXPECT_TRUE(layoutLines(&beats,
            audio::FramePos(),
            audio::FramePos(100000),
            kFrameToX,
            LineSettings(),
            90,
            kSkinColor)
                        .empty());
    EXPECT_TRUE(lines(beats, 100000, 0).empty());
    // A range between two beats.
    EXPECT_TRUE(lines(beats, 500, 400 + kBeatFrames - 1).empty());
    // Both ends are inclusive.
    expectLines({400}, "B", lines(beats, 400, 400));
    expectLines({400 + kBeatFrames, 400 + 2 * kBeatFrames},
            "bb",
            lines(beats, 400 + kBeatFrames, 400 + 2 * kBeatFrames));
}

TEST(ScrollingLineLayoutTest, DefaultWidthsAreBar2Beat1) {
    const Beats beats = constBeats(400);
    const auto result = lines(beats, 0, 400 + 1 * kBeatFrames);
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ(2, result[0].widthPx);
    EXPECT_EQ(1, result[1].widthPx);
}

TEST(ScrollingLineLayoutTest, WidthsAreClampedTo1To5) {
    const Beats beats = constBeats(400);
    LineSettings settings;
    settings.barWidth = LineWidthPx(9);
    settings.beatWidth = LineWidthPx(0);
    auto result = lines(beats, 0, 400 + 1 * kBeatFrames, settings);
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ(5, result[0].widthPx);
    EXPECT_EQ(1, result[1].widthPx);

    settings.barWidth = LineWidthPx(3);
    settings.beatWidth = LineWidthPx(4);
    result = lines(beats, 0, 400 + 1 * kBeatFrames, settings);
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ(3, result[0].widthPx);
    EXPECT_EQ(4, result[1].widthPx);
}

TEST(ScrollingLineLayoutTest, LinesUseTheSkinColourByDefault) {
    const Beats beats = constBeats(400);
    const auto result = lines(beats, 0, 400 + 1 * kBeatFrames);
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ(kSkinColor, result[0].color);
    EXPECT_EQ(kSkinColor, result[1].color);
}

TEST(ScrollingLineLayoutTest, CustomColourOverridesTheSkinColourPerKind) {
    const Beats beats = constBeats(400);
    LineSettings settings;
    settings.barColor = QColor(255, 0, 0);
    auto result = lines(beats, 0, 400 + 1 * kBeatFrames, settings);
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ(QColor(255, 0, 0), result[0].color);
    EXPECT_EQ(kSkinColor, result[1].color);

    settings.barColor.reset();
    settings.beatColor = QColor(0, 255, 0);
    result = lines(beats, 0, 400 + 1 * kBeatFrames, settings);
    ASSERT_EQ(2u, result.size());
    EXPECT_EQ(kSkinColor, result[0].color);
    EXPECT_EQ(QColor(0, 255, 0), result[1].color);
}

TEST(ScrollingLineLayoutTest, MasterSwitchOffIgnoresWidthAndColour) {
    // Upstream draws 1 px lines in the skin's beat colour.
    const Beats beats = constBeats(400);
    LineSettings settings;
    settings.enabled = false;
    settings.barWidth = LineWidthPx(4);
    settings.beatWidth = LineWidthPx(3);
    settings.barColor = QColor(255, 0, 0);
    settings.beatColor = QColor(0, 255, 0);
    const auto result = lines(beats, 0, 400 + 5 * kBeatFrames, settings);
    ASSERT_EQ(6u, result.size());
    for (const auto& line : result) {
        EXPECT_EQ(LineKind::Beat, line.kind);
        EXPECT_EQ(1, line.widthPx);
        EXPECT_EQ(kSkinColor, line.color);
    }
}

TEST(ScrollingLineLayoutTest, StepLinesAtQuartersBetweenBeats) {
    const Beats beats = constBeats(400);
    // Beats 0 and 1 are 240 px apart, so steps are 60 px apart.
    const auto result = lines(beats, 0, 400 + kBeatFrames, stepSettings());
    expectLines({400, 6400, 12400, 18400, 400 + kBeatFrames}, "Bsssb", result);
}

TEST(ScrollingLineLayoutTest, StepLinesBeforeTheFirstBeatInRange) {
    const Beats beats = constBeats(400);
    // The range starts between beats 0 and 1, after the first step.
    const auto result = lines(beats, 7000, 400 + kBeatFrames, stepSettings());
    expectLines({12400, 18400, 400 + kBeatFrames}, "ssb", result);
    // And ends before beat 2, after the first step after beat 1.
    const auto tail = lines(beats, 400 + kBeatFrames, 400 + 1.3 * kBeatFrames, stepSettings());
    expectLines({400 + kBeatFrames, 400 + 1.25 * kBeatFrames}, "bs", tail);
}

TEST(ScrollingLineLayoutTest, StepLinesFollowATempoChange) {
    // Beat 7 at 120 BPM, beat 8 starts 60 BPM (48000 frames apart).
    const double tempoChange = 400 + 8 * kBeatFrames;
    const auto result = lines(kTempoChangeBeats,
            400 + 7 * kBeatFrames,
            tempoChange + 48000,
            stepSettings());
    expectLines({400 + 7 * kBeatFrames,
                        400 + 7.25 * kBeatFrames,
                        400 + 7.5 * kBeatFrames,
                        400 + 7.75 * kBeatFrames,
                        tempoChange,
                        tempoChange + 12000,
                        tempoChange + 24000,
                        tempoChange + 36000,
                        tempoChange + 48000},
            "bsssBsssb",
            result);
}

namespace {

// Lines for one beat from frame 0 of a constant grid at the given tempo, with
// the beat 'beatPx' wide on screen.
std::vector<Line> oneBeatAtWidth(double bpm, double beatPx, const LineSettings& settings) {
    const Beats beats(audio::FramePos(0), Bpm(bpm), kSampleRate, QString());
    const double beatFrames = kSampleRate.value() * 60.0 / bpm;
    return layoutLines(&beats,
            audio::FramePos(0),
            audio::FramePos(beatFrames),
            [=](audio::FramePos pos) { return pos.value() * beatPx / beatFrames; },
            settings,
            90,
            kSkinColor);
}

size_t stepCount(const std::vector<Line>& result) {
    return std::count_if(result.begin(), result.end(), [](const Line& line) {
        return line.kind == LineKind::Step;
    });
}

} // namespace

TEST(ScrollingLineLayoutTest, StepLinesOnlyWhenAtLeastTheMinimumSpacingApart) {
    // Default minimum spacing 8 px: a beat must be at least 32 px wide.
    for (const double bpm : {120.0, 174.0}) {
        EXPECT_EQ(3u, stepCount(oneBeatAtWidth(bpm, 32.01, stepSettings()))) << bpm;
        EXPECT_EQ(0u, stepCount(oneBeatAtWidth(bpm, 31.99, stepSettings()))) << bpm;
        // Beat lines are drawn either way.
        EXPECT_EQ(2u, oneBeatAtWidth(bpm, 31.99, stepSettings()).size()) << bpm;
    }
    LineSettings settings = stepSettings();
    settings.stepMinSpacing = StepSpacingPx(20);
    EXPECT_EQ(3u, stepCount(oneBeatAtWidth(120, 80.01, settings)));
    EXPECT_EQ(0u, stepCount(oneBeatAtWidth(120, 79.99, settings)));
}

TEST(ScrollingLineLayoutTest, StepSpacingIsClampedTo4To30) {
    EXPECT_EQ(8, StepSpacingPx().px());
    EXPECT_EQ(4, StepSpacingPx(1).px());
    EXPECT_EQ(30, StepSpacingPx(100).px());
    LineSettings settings = stepSettings();
    settings.stepMinSpacing = StepSpacingPx(1);
    EXPECT_EQ(3u, stepCount(oneBeatAtWidth(120, 16.01, settings)));
    EXPECT_EQ(0u, stepCount(oneBeatAtWidth(120, 15.99, settings)));
    settings.stepMinSpacing = StepSpacingPx(100);
    EXPECT_EQ(3u, stepCount(oneBeatAtWidth(120, 120.01, settings)));
    EXPECT_EQ(0u, stepCount(oneBeatAtWidth(120, 119.99, settings)));
}

TEST(ScrollingLineLayoutTest, NoStepLinesWithTheSwitchOrMasterOff) {
    const Beats beats = constBeats(400);
    EXPECT_EQ(0u, stepCount(lines(beats, 0, 400 + 2 * kBeatFrames)));
    LineSettings settings = stepSettings();
    settings.enabled = false;
    EXPECT_EQ(0u, stepCount(lines(beats, 0, 400 + 2 * kBeatFrames, settings)));
    settings = stepSettings();
    settings.stepOpacity = OpacityPercent(0);
    EXPECT_EQ(0u, stepCount(lines(beats, 0, 400 + 2 * kBeatFrames, settings)));
}

TEST(ScrollingLineLayoutTest, StepLineOpacityWidthAndColour) {
    const Beats beats = constBeats(400);
    LineSettings settings = stepSettings();
    auto result = lines(beats, 0, 400 + kBeatFrames, settings);
    ASSERT_EQ(5u, result.size());
    EXPECT_EQ(45, result[1].opacityPercent);
    EXPECT_EQ(1, result[1].widthPx);
    EXPECT_EQ(kSkinColor, result[1].color);

    settings.stepOpacity = OpacityPercent(30);
    settings.stepWidth = LineWidthPx(3);
    settings.stepColor = QColor(0, 0, 255);
    result = lines(beats, 0, 400 + kBeatFrames, settings);
    ASSERT_EQ(5u, result.size());
    for (int i = 1; i <= 3; ++i) {
        EXPECT_EQ(30, result[i].opacityPercent);
        EXPECT_EQ(3, result[i].widthPx);
        EXPECT_EQ(QColor(0, 0, 255), result[i].color);
    }
    // Bar and beat lines keep their own look.
    EXPECT_EQ(2, result[0].widthPx);
    EXPECT_EQ(kSkinColor, result[4].color);
}
