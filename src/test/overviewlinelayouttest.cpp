#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "audio/frame.h"
#include "audio/types.h"
#include "track/beats.h"
#include "track/bpm.h"
#include "waveform/overviewlinelayout.h"

namespace {

using namespace mixxx;
using namespace mixxx::overviewlayout;

constexpr auto kBpm = Bpm(120.0);
constexpr auto kSampleRate = audio::SampleRate(48000);
// A beat at 120 BPM / 48 kHz is 24000 frames; a bar is 96000 frames.
constexpr double kBeatFrames = 24000.0;

// 50 s track (100 beats at 120 BPM), 1002 px overview so that the drawable
// range (length - 2) is 1000 px.
constexpr double kTrackSamples = 2.0 * 2400000.0;
constexpr double kLength = 1002.0;

// Same mapping the overview uses for cue marks.
double px(double frame) {
    return 1.0 + 2.0 * frame * (kLength - 2.0) / kTrackSamples;
}

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

LineSettings settings(LinesMode mode, int phraseBars = 8) {
    LineSettings s;
    s.mode = mode;
    s.phraseBars = phraseBars;
    return s;
}

void expectPositions(const std::vector<double>& expected, const std::vector<double>& actual) {
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(expected[i], actual[i], 1e-6) << "index " << i;
    }
}

} // namespace

TEST(OverviewLineLayoutTest, DefaultsArePhrasesOnlyEightBars) {
    const LineSettings s;
    EXPECT_EQ(LinesMode::PhrasesOnly, s.mode);
    EXPECT_EQ(8, s.phraseBars);
}

TEST(OverviewLineLayoutTest, PhrasesOnlyConstantTempo) {
    const Beats beats = constBeats(400);
    const auto lines = layoutLines(&beats, kTrackSamples, kLength, settings(LinesMode::PhrasesOnly));
    EXPECT_TRUE(lines.barLines.empty());
    // 8 bars = 32 beats: beat indices 0, 32, 64, 96.
    expectPositions({px(400),
                            px(400 + 32 * kBeatFrames),
                            px(400 + 64 * kBeatFrames),
                            px(400 + 96 * kBeatFrames)},
            lines.phraseLines);
}

TEST(OverviewLineLayoutTest, OffShowsNoLines) {
    const Beats beats = constBeats(400);
    const auto lines = layoutLines(&beats, kTrackSamples, kLength, settings(LinesMode::Off));
    EXPECT_TRUE(lines.barLines.empty());
    EXPECT_TRUE(lines.phraseLines.empty());
}

TEST(OverviewLineLayoutTest, NoBeatGridShowsNoLines) {
    const auto lines = layoutLines(nullptr, kTrackSamples, kLength, settings(LinesMode::BarsAndPhrases));
    EXPECT_TRUE(lines.barLines.empty());
    EXPECT_TRUE(lines.phraseLines.empty());
}

TEST(OverviewLineLayoutTest, EmptyTrackShowsNoLines) {
    const Beats beats = constBeats(400);
    const auto lines = layoutLines(&beats, 0.0, kLength, settings(LinesMode::BarsAndPhrases));
    EXPECT_TRUE(lines.barLines.empty());
    EXPECT_TRUE(lines.phraseLines.empty());
}

TEST(OverviewLineLayoutTest, BarsAndPhrasesHaveNoDuplicateOnPhraseBoundary) {
    const Beats beats = constBeats(400);
    const auto lines = layoutLines(&beats, kTrackSamples, kLength, settings(LinesMode::BarsAndPhrases, 4));
    // Phrases (4 bars = 16 beats) at beat indices 0, 16, ..., 96.
    ASSERT_EQ(7u, lines.phraseLines.size());
    // Bars at every 4th beat 0..96 = 25, minus the 7 phrase starts.
    ASSERT_EQ(18u, lines.barLines.size());
    for (double barPos : lines.barLines) {
        for (double phrasePos : lines.phraseLines) {
            EXPECT_GT(std::abs(barPos - phrasePos), 1e-6);
        }
    }
    // Bar at beat index 4 is the first bar line.
    EXPECT_NEAR(px(400 + 4 * kBeatFrames), lines.barLines.front(), 1e-6);
}

TEST(OverviewLineLayoutTest, EachPhraseLength) {
    const Beats beats = constBeats(400);
    const struct {
        int bars;
        size_t expectedPhraseLines;
    } cases[] = {{4, 7}, {8, 4}, {16, 2}, {32, 1}};
    for (const auto& c : cases) {
        const auto lines = layoutLines(&beats,
                kTrackSamples,
                kLength,
                settings(LinesMode::PhrasesOnly, c.bars));
        EXPECT_EQ(c.expectedPhraseLines, lines.phraseLines.size()) << c.bars << " bars";
    }
}

TEST(OverviewLineLayoutTest, TempoChangeFollowsTheGrid) {
    const auto lines = layoutLines(
            &kTempoChangeBeats, kTrackSamples, kLength, settings(LinesMode::BarsAndPhrases, 4));
    // Beat frames: idx 0..8 every 24000 from 400, idx 8..24 every 48000 from
    // 192400, idx 24+ every 24000 from 960400. Bars every 4 beats; phrases
    // (16 beats) at idx 0, 16, 32, 48, 64, 80. The track ends at idx 84.
    const double marker1 = 400 + 8 * 24000;
    const double marker2 = marker1 + 16 * 48000;
    const std::vector<double> phrases = {
            px(400),
            px(marker1 + 8 * 48000), // idx 16
            px(marker2 + 8 * 24000), // idx 32
            px(marker2 + 24 * 24000), // idx 48
            px(marker2 + 40 * 24000), // idx 64
            px(marker2 + 56 * 24000), // idx 80
    };
    expectPositions(phrases, lines.phraseLines);
    // Bars at idx 4, 8, 12, 20, 24, 28, ...
    ASSERT_GE(lines.barLines.size(), 6u);
    EXPECT_NEAR(px(400 + 4 * 24000), lines.barLines[0], 1e-6);
    EXPECT_NEAR(px(marker1), lines.barLines[1], 1e-6);
    EXPECT_NEAR(px(marker1 + 4 * 48000), lines.barLines[2], 1e-6);
    EXPECT_NEAR(px(marker1 + 12 * 48000), lines.barLines[3], 1e-6);
    EXPECT_NEAR(px(marker2), lines.barLines[4], 1e-6);
    EXPECT_NEAR(px(marker2 + 4 * 24000), lines.barLines[5], 1e-6);
}

TEST(OverviewLineLayoutTest, LinesContinueBackThroughTheIntro) {
    // The grid's first beat (idx 0) is at frame 960400; the grid is
    // extrapolated backwards, so idx -40 is at frame 400.
    const Beats beats = constBeats(400 + 40 * kBeatFrames);
    const auto lines = layoutLines(&beats, kTrackSamples, kLength, settings(LinesMode::BarsAndPhrases, 8));
    // Phrase (32 beats) lines at idx -32, 0 and 32; the first is before the
    // first beat.
    ASSERT_EQ(3u, lines.phraseLines.size());
    EXPECT_NEAR(px(400 + 8 * kBeatFrames), lines.phraseLines[0], 1e-6);
    EXPECT_NEAR(px(400 + 40 * kBeatFrames), lines.phraseLines[1], 1e-6);
    EXPECT_NEAR(px(400 + 72 * kBeatFrames), lines.phraseLines[2], 1e-6);
    // The first bar line is idx -40 (frame 400); idx -36 follows.
    EXPECT_NEAR(px(400), lines.barLines.front(), 1e-6);
    EXPECT_NEAR(px(400 + 4 * kBeatFrames), lines.barLines[1], 1e-6);
}

TEST(OverviewLineLayoutTest, LinesAreInsideTheTrack) {
    const Beats beats = constBeats(400);
    const auto lines = layoutLines(&beats, kTrackSamples, kLength, settings(LinesMode::BarsAndPhrases, 4));
    for (double p : lines.barLines) {
        EXPECT_GE(p, 1.0);
        EXPECT_LE(p, kLength - 1.0);
    }
    for (double p : lines.phraseLines) {
        EXPECT_GE(p, 1.0);
        EXPECT_LE(p, kLength - 1.0);
    }
}
