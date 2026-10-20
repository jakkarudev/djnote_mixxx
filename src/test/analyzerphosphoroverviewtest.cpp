#include <gtest/gtest.h>

#include <QDir>
#include <algorithm>
#include <cstdlib>
#include <vector>

#include "analyzer/analyzerphosphoroverview.h"
#include "analyzer/analyzertrack.h"
#include "test/mixxxtest.h"
#include "test/phosphoroverviewfixture.h"
#include "track/track.h"
#include "waveform/phosphor/phosphoroverview.h"
#include "widget/woverview.h"

namespace {

using namespace mixxx::phosphor;
namespace pf = phosphoroverviewfixture;

class AnalyzerPhosphorOverviewTest : public MixxxTest {
  protected:
    void selectOverviewType(WOverview::Type type) {
        config()->setValue(ConfigKey("[Waveform]", "WaveformOverviewType"), type);
    }

    pf::Fixture load(const QString& name) const {
        return pf::load(getTestDir().filePath("phosphor/" + name));
    }

    // Float interleaved stereo like the analyzer thread delivers it.
    static std::vector<CSAMPLE> toStereoFloats(const pf::Fixture& f) {
        std::vector<CSAMPLE> out;
        out.reserve(static_cast<size_t>(f.frames) * 2);
        for (uint32_t i = 0; i < f.frames; ++i) {
            out.push_back(static_cast<CSAMPLE>(f.input[i * f.channels]) / 32768.0f);
            out.push_back(static_cast<CSAMPLE>(f.input[i * f.channels + (f.channels - 1)]) /
                    32768.0f);
        }
        return out;
    }

    static void feed(AnalyzerPhosphorOverview* pAnalyzer,
            const std::vector<CSAMPLE>& stereo,
            SINT from,
            SINT to,
            SINT samplesPerChunk) {
        for (SINT pos = from; pos < to; pos += samplesPerChunk) {
            const SINT n = std::min<SINT>(samplesPerChunk, to - pos);
            ASSERT_TRUE(pAnalyzer->processSamples(stereo.data() + pos, n));
        }
    }
};

// Analyzer seam: the fixtures' stored columns.
TEST_F(AnalyzerPhosphorOverviewTest, ColumnsMatchReferenceFixtures) {
    selectOverviewType(WOverview::Type::Phosphor);
    const QStringList files = QDir(getTestDir().filePath("phosphor"))
                                      .entryList({"*.phov"}, QDir::Files, QDir::Name);
    ASSERT_FALSE(files.isEmpty());
    for (const QString& file : files) {
        const auto f = load(file);
        ASSERT_TRUE(f.ok) << file.toStdString();
        const TrackPointer pTrack = Track::newTemporary();
        AnalyzerPhosphorOverview analyzer(config());
        ASSERT_TRUE(analyzer.initialize(AnalyzerTrack(pTrack),
                mixxx::audio::SampleRate(f.rate),
                f.frames))
                << file.toStdString();
        const auto stereo = toStereoFloats(f);
        feed(&analyzer, stereo, 0, static_cast<SINT>(stereo.size()), 2 * 4097);
        analyzer.storeResults(pTrack);
        analyzer.cleanup();

        const auto pData = pTrack->getPhosphorOverview();
        ASSERT_TRUE(pData) << file.toStdString();
        EXPECT_EQ(kOverviewColumns, pData->completedColumns());
        EXPECT_EQ(static_cast<int>(f.samplesPerColumn), pData->samplesPerColumn());
        const auto view = pData->view();
        int worstLevel = 0;
        int worstDensity = 0;
        for (size_t i = 0; i < f.levels.size(); ++i) {
            worstLevel = std::max(worstLevel, std::abs(int(view.levels[i]) - int(f.levels[i])));
        }
        for (size_t i = 0; i < f.density.size(); ++i) {
            worstDensity = std::max(
                    worstDensity, std::abs(int(view.density[i]) - int(f.density[i])));
        }
        EXPECT_LE(worstLevel, 1) << file.toStdString();
        EXPECT_LE(worstDensity, 1) << file.toStdString();
    }
}

TEST_F(AnalyzerPhosphorOverviewTest, DoesNothingUnlessPhosphorIsSelected) {
    const auto f = load("q4_sine_vs_square.phov");
    ASSERT_TRUE(f.ok);
    for (WOverview::Type type : {WOverview::Type::Filtered,
                 WOverview::Type::HSV,
                 WOverview::Type::RGB}) {
        selectOverviewType(type);
        const TrackPointer pTrack = Track::newTemporary();
        AnalyzerPhosphorOverview analyzer(config());
        EXPECT_FALSE(analyzer.initialize(
                AnalyzerTrack(pTrack), mixxx::audio::SampleRate(f.rate), f.frames));
        EXPECT_FALSE(pTrack->getPhosphorOverview());
    }
}

TEST_F(AnalyzerPhosphorOverviewTest, PublishesProgressWhileDecodingAndSharesTheData) {
    selectOverviewType(WOverview::Type::Phosphor);
    const auto f = load("q4_sine_vs_square.phov");
    ASSERT_TRUE(f.ok);
    const auto stereo = toStereoFloats(f);
    const SINT half = static_cast<SINT>(stereo.size() / 2);
    const TrackPointer pTrack = Track::newTemporary();
    AnalyzerPhosphorOverview analyzer(config());
    ASSERT_TRUE(analyzer.initialize(
            AnalyzerTrack(pTrack), mixxx::audio::SampleRate(f.rate), f.frames));
    feed(&analyzer, stereo, 0, half, 2 * 1000);
    const auto pData = pTrack->getPhosphorOverview();
    ASSERT_TRUE(pData);
    EXPECT_GT(pData->completedColumns(), 0);
    EXPECT_LT(pData->completedColumns(), kOverviewColumns / 2); // the color track trails

    // A second request while the first is running (or after it) builds nothing.
    AnalyzerPhosphorOverview second(config());
    EXPECT_FALSE(second.initialize(
            AnalyzerTrack(pTrack), mixxx::audio::SampleRate(f.rate), f.frames));
    EXPECT_EQ(pData, pTrack->getPhosphorOverview());

    feed(&analyzer, stereo, half, static_cast<SINT>(stereo.size()), 2 * 1000);
    analyzer.storeResults(pTrack);
    analyzer.cleanup();
    EXPECT_TRUE(pTrack->getPhosphorOverview()->isComplete());
    EXPECT_EQ(pData, pTrack->getPhosphorOverview());
}

TEST_F(AnalyzerPhosphorOverviewTest, AbortedBuildLeavesNothingAndALengthChangeRebuilds) {
    selectOverviewType(WOverview::Type::Phosphor);
    const auto f = load("q4_sine_vs_square.phov");
    ASSERT_TRUE(f.ok);
    const auto stereo = toStereoFloats(f);
    const TrackPointer pTrack = Track::newTemporary();
    {
        AnalyzerPhosphorOverview analyzer(config());
        ASSERT_TRUE(analyzer.initialize(
                AnalyzerTrack(pTrack), mixxx::audio::SampleRate(f.rate), f.frames));
        feed(&analyzer, stereo, 0, 2 * 20000, 2 * 1000);
        analyzer.cleanup(); // aborted
    }
    EXPECT_FALSE(pTrack->getPhosphorOverview());

    AnalyzerPhosphorOverview analyzer(config());
    ASSERT_TRUE(analyzer.initialize(
            AnalyzerTrack(pTrack), mixxx::audio::SampleRate(f.rate), f.frames));
    feed(&analyzer, stereo, 0, static_cast<SINT>(stereo.size()), 2 * 4096);
    analyzer.storeResults(pTrack);
    analyzer.cleanup();
    ASSERT_TRUE(pTrack->getPhosphorOverview());

    // The file got a different length: the columns would land at the wrong time.
    AnalyzerPhosphorOverview again(config());
    EXPECT_TRUE(again.initialize(
            AnalyzerTrack(pTrack), mixxx::audio::SampleRate(f.rate), f.frames + 5000));
    again.cleanup();
}

} // namespace
