#include <gtest/gtest.h>

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

} // namespace
