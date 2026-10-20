#include <gtest/gtest.h>

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
            upstreamOpacity);
}

// Checks positions and kinds; 'B' = bar line, 'b' = beat line.
void expectLines(const std::vector<double>& expectedFrames,
        const std::string& expectedKinds,
        const std::vector<Line>& actual) {
    ASSERT_EQ(expectedFrames.size(), actual.size());
    ASSERT_EQ(expectedKinds.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i) {
        EXPECT_NEAR(px(expectedFrames[i]), actual[i].x, 1e-6) << "index " << i;
        const LineKind kind = expectedKinds[i] == 'B' ? LineKind::Bar : LineKind::Beat;
        EXPECT_EQ(kind, actual[i].kind) << "index " << i;
    }
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
            90)
                        .empty());
    // Invalid or reversed range.
    EXPECT_TRUE(layoutLines(&beats,
            audio::FramePos(),
            audio::FramePos(100000),
            kFrameToX,
            LineSettings(),
            90)
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
