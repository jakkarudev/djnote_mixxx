#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

#include "analyzer/analyzerphosphor.h"
#include "analyzer/analyzertrack.h"
#include "test/mixxxtest.h"
#include "test/phosphorfixture.h"
#include "track/track.h"
#include "waveform/phosphor/phosphordata.h"

namespace {

const char* const kFixtures[] = {
    "q2a_630hz",
    "q2b_sweep",
    "q3a_100hz_step",
    "q4_silence",
    "q4_sine",
    "q4_square",
    "q5_click",
    "q5_kick",
    "q6_noise",
    "q7_inverted",
    "q9_mixture",
};

class AnalyzerPhosphorTest : public MixxxTest {
  protected:
    static AnalyzerTrack::Options deckOptions() {
        AnalyzerTrack::Options options;
        options.buildPhosphor = true;
        return options;
    }

    // Float interleaved stereo like the analyzer thread delivers it (mono
    // files arrive duplicated to both channels).
    static std::vector<CSAMPLE> toStereoFloats(const phosphorfixture::Fixture& f) {
        std::vector<CSAMPLE> out;
        out.reserve(static_cast<size_t>(f.frames) * 2);
        for (uint32_t i = 0; i < f.frames; ++i) {
            const int16_t l = f.input[i * f.channels];
            const int16_t r = f.input[i * f.channels + (f.channels - 1)];
            out.push_back(static_cast<CSAMPLE>(l) / 32768.0f);
            out.push_back(static_cast<CSAMPLE>(r) / 32768.0f);
        }
        return out;
    }

    static bool feed(AnalyzerPhosphor* pAnalyzer,
            const std::vector<CSAMPLE>& stereo,
            SINT samplesPerChunk) {
        for (SINT pos = 0; pos < static_cast<SINT>(stereo.size()); pos += samplesPerChunk) {
            const SINT n = std::min<SINT>(samplesPerChunk, stereo.size() - pos);
            if (!pAnalyzer->processSamples(stereo.data() + pos, n)) {
                return false;
            }
        }
        return true;
    }
};

TEST_F(AnalyzerPhosphorTest, FullBufferMatchesReferenceFixtures) {
    for (const char* name : kFixtures) {
        const auto f = phosphorfixture::load(
                getTestDir().filePath(QStringLiteral("phosphor/%1.phfx").arg(name)));
        ASSERT_TRUE(f.ok) << name;
        TrackPointer pTrack = Track::newTemporary();
        AnalyzerPhosphor analyzer(config());
        ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack, deckOptions()),
                mixxx::audio::SampleRate(f.rate),
                f.frames))
                << name;
        // 4096 frames per chunk like the analyzer thread; the fixtures are
        // shorter, so also test with small odd chunks.
        ASSERT_TRUE(feed(&analyzer, toStereoFloats(f), 2 * 1001)) << name;
        analyzer.storeResults(pTrack);
        analyzer.cleanup();

        const auto pData = pTrack->getPhosphorData();
        ASSERT_TRUE(pData) << name;
        EXPECT_TRUE(pData->isFinished()) << name;
        EXPECT_EQ(static_cast<int>(f.rate), pData->sampleRate()) << name;
        ASSERT_EQ(static_cast<SINT>(f.frames), pData->totalFrames()) << name;
        ASSERT_EQ(static_cast<SINT>(f.frames), pData->pointsCompleted()) << name;
        // Expected values from the reference script: floor((L + R) / 2 + 0.5)
        for (uint32_t i = 0; i < f.frames; ++i) {
            const int l = f.input[i * f.channels];
            const int r = f.input[i * f.channels + (f.channels - 1)];
            const int expected = std::clamp(
                    static_cast<int>(std::floor((l + r) / 2.0 + 0.5)), -32768, 32767);
            ASSERT_EQ(expected, pData->pointValue(i)) << name << " frame " << i;
        }
    }
}

TEST_F(AnalyzerPhosphorTest, ProgressIsPublishedWhileDecoding) {
    const auto f = phosphorfixture::load(getTestDir().filePath("phosphor/q4_sine.phfx"));
    ASSERT_TRUE(f.ok);
    TrackPointer pTrack = Track::newTemporary();
    AnalyzerPhosphor analyzer(config());
    ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack, deckOptions()),
            mixxx::audio::SampleRate(f.rate),
            f.frames));
    // The data is handed to the track right away, still empty.
    const auto pData = pTrack->getPhosphorData();
    ASSERT_TRUE(pData);
    EXPECT_EQ(0, pData->pointsCompleted());
    EXPECT_FALSE(pData->isFinished());

    const auto stereo = toStereoFloats(f);
    ASSERT_TRUE(analyzer.processSamples(stereo.data(), 2 * 1000));
    EXPECT_EQ(1000, pData->pointsCompleted());
    EXPECT_FALSE(pData->isFinished());
    ASSERT_TRUE(analyzer.processSamples(stereo.data() + 2 * 1000, 2 * 500));
    EXPECT_EQ(1500, pData->pointsCompleted());
}

TEST_F(AnalyzerPhosphorTest, OnlyRunsWhenAskedByAnOption) {
    TrackPointer pTrack = Track::newTemporary();
    AnalyzerPhosphor analyzer(config());
    // Samplers, preview decks and library batch analysis use default options.
    EXPECT_FALSE(analyzer.initialize(AnalyzerTrack(pTrack),
            mixxx::audio::SampleRate(44100),
            44100));
    EXPECT_FALSE(pTrack->getPhosphorData());
}

TEST_F(AnalyzerPhosphorTest, SecondLoadOfSameTrackSharesTheBuffer) {
    TrackPointer pTrack = Track::newTemporary();
    AnalyzerPhosphor first(config());
    ASSERT_TRUE(first.initialize(AnalyzerTrack(pTrack, deckOptions()),
            mixxx::audio::SampleRate(44100),
            44100));
    const auto pData = pTrack->getPhosphorData();
    ASSERT_TRUE(pData);

    // Still decoding for the first deck.
    AnalyzerPhosphor second(config());
    EXPECT_FALSE(second.initialize(AnalyzerTrack(pTrack, deckOptions()),
            mixxx::audio::SampleRate(44100),
            44100));
    EXPECT_EQ(pData, pTrack->getPhosphorData());

    // And after it finished.
    first.storeResults(pTrack);
    first.cleanup();
    AnalyzerPhosphor third(config());
    EXPECT_FALSE(third.initialize(AnalyzerTrack(pTrack, deckOptions()),
            mixxx::audio::SampleRate(44100),
            44100));
    EXPECT_EQ(pData, pTrack->getPhosphorData());
}

TEST_F(AnalyzerPhosphorTest, AbortedAnalysisDropsPartialDataSoTheNextLoadRetries) {
    TrackPointer pTrack = Track::newTemporary();
    AnalyzerPhosphor analyzer(config());
    ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack, deckOptions()),
            mixxx::audio::SampleRate(44100),
            44100));
    ASSERT_TRUE(pTrack->getPhosphorData());
    analyzer.cleanup(); // no storeResults: aborted
    EXPECT_FALSE(pTrack->getPhosphorData());
}

TEST_F(AnalyzerPhosphorTest, MoreSamplesThanAnnouncedAreDropped) {
    TrackPointer pTrack = Track::newTemporary();
    AnalyzerPhosphor analyzer(config());
    ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack, deckOptions()),
            mixxx::audio::SampleRate(44100),
            100));
    const std::vector<CSAMPLE> stereo(2 * 300, 0.5f);
    ASSERT_TRUE(analyzer.processSamples(stereo.data(), stereo.size()));
    analyzer.storeResults(pTrack);
    analyzer.cleanup();
    EXPECT_EQ(100, pTrack->getPhosphorData()->totalFrames());
}

TEST_F(AnalyzerPhosphorTest, ShorterFileThanAnnouncedTrimsTheLength) {
    TrackPointer pTrack = Track::newTemporary();
    AnalyzerPhosphor analyzer(config());
    ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack, deckOptions()),
            mixxx::audio::SampleRate(44100),
            1000));
    const std::vector<CSAMPLE> stereo(2 * 300, 0.5f);
    ASSERT_TRUE(analyzer.processSamples(stereo.data(), stereo.size()));
    analyzer.storeResults(pTrack);
    analyzer.cleanup();
    EXPECT_EQ(300, pTrack->getPhosphorData()->totalFrames());
    EXPECT_EQ(300, pTrack->getPhosphorData()->pointsCompleted());
}

// The colour track through the analyzer, against the reference script's
// fixtures. Level = u16 / 65535; the error is absolute, relative to the
// loudest level of that channel in the fixture.
TEST_F(AnalyzerPhosphorTest, ColourTrackMatchesReferenceFixtures) {
    const char* const names[] = {"q2a_630hz",
            "q2b_sweep",
            "q3a_100hz_step",
            "q5_click",
            "q5_kick",
            "q9_mixture",
            "q4_sine"};
    double worst = 0;
    for (const char* name : names) {
        const auto f = phosphorfixture::load(
                getTestDir().filePath(QStringLiteral("phosphor/%1.phfx").arg(name)));
        ASSERT_TRUE(f.ok) << name;
        TrackPointer pTrack = Track::newTemporary();
        AnalyzerPhosphor analyzer(config());
        ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack, deckOptions()),
                mixxx::audio::SampleRate(f.rate),
                f.frames));
        ASSERT_TRUE(feed(&analyzer, toStereoFloats(f), 2 * 1001)) << name;
        analyzer.storeResults(pTrack);
        analyzer.cleanup();
        const auto pData = pTrack->getPhosphorData();
        ASSERT_TRUE(pData);
        ASSERT_EQ(static_cast<SINT>(f.colourBlockFrames), pData->colourBlockFrames()) << name;
        const size_t blocks = f.colour.size() / 3;
        ASSERT_EQ(static_cast<SINT>(blocks), pData->colourBlocksCompleted()) << name;
        for (int c = 0; c < 3; ++c) {
            double peak = 1e-9;
            for (size_t b = 0; b < blocks; ++b) {
                peak = std::max(peak, f.colour[b * 3 + c] / 65535.0);
            }
            for (size_t b = 0; b < blocks; ++b) {
                const double expected = f.colour[b * 3 + c] / 65535.0;
                const double actual = pData->colourLevel(static_cast<SINT>(b), c) / 65535.0;
                const double err = std::abs(expected - actual) / peak;
                worst = std::max(worst, err);
                ASSERT_LE(err, 0.01) << name << " channel " << c << " block " << b;
            }
        }
    }
    std::cout << "colour track worst error vs fixtures (relative to channel peak): " << worst
              << std::endl;
}

TEST_F(AnalyzerPhosphorTest, AnalyzerCostPerMinuteOfAudio) {
    const int rate = 44100;
    const SINT frames = 60 * rate;
    TrackPointer pTrack = Track::newTemporary();
    AnalyzerPhosphor analyzer(config());
    ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack, deckOptions()),
            mixxx::audio::SampleRate(rate),
            frames));
    std::vector<CSAMPLE> stereo(2 * frames);
    uint32_t state = 12345;
    for (auto& v : stereo) {
        state = state * 1664525u + 1013904223u;
        v = (static_cast<int>(state >> 16) - 32768) / 65536.0f;
    }
    const auto start = std::chrono::steady_clock::now();
    ASSERT_TRUE(feed(&analyzer, stereo, 2 * 4096));
    analyzer.storeResults(pTrack);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
                            .count();
    analyzer.cleanup();
    std::cout << "Phosphor analyzer: " << ms << " ms per minute of audio" << std::endl;
    EXPECT_EQ(frames / 44, pTrack->getPhosphorData()->colourBlocksCompleted());
}

TEST_F(AnalyzerPhosphorTest, ColourTrackFillsProgressively) {
    const int rate = 44100;
    const SINT frames = 3 * rate;
    TrackPointer pTrack = Track::newTemporary();
    AnalyzerPhosphor analyzer(config());
    ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack, deckOptions()),
            mixxx::audio::SampleRate(rate),
            frames));
    const auto pData = pTrack->getPhosphorData();
    std::vector<CSAMPLE> stereo(2 * frames);
    for (SINT i = 0; i < frames; ++i) {
        stereo[2 * i] = stereo[2 * i + 1] =
                0.5f * static_cast<float>(std::sin(2 * M_PI * 100 * i / rate));
    }
    SINT midBlocks = 0;
    for (SINT pos = 0; pos < 2 * frames; pos += 2 * 4096) {
        ASSERT_TRUE(analyzer.processSamples(
                stereo.data() + pos, std::min<SINT>(2 * 4096, 2 * frames - pos)));
        if (pos == 2 * 4096 * 20) {
            midBlocks = pData->colourBlocksCompleted();
        }
    }
    // Blocks appear while decoding, behind the samples.
    EXPECT_GT(midBlocks, 0);
    EXPECT_LT(midBlocks, 20 * 4096 / 44);
    analyzer.storeResults(pTrack);
    analyzer.cleanup();
    EXPECT_EQ(frames / 44, pData->colourBlocksCompleted());
    // A 100 Hz tone is red, not blue.
    EXPECT_GT(pData->colourLevel(1500, 0), 3 * pData->colourLevel(1500, 2));
}

} // namespace
