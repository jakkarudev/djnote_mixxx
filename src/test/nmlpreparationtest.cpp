#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "analyzer/analyzerbeats.h"
#include "analyzer/analyzertrack.h"
#include "library/dao/playlistdao.h"
#include "library/nml/nmlconstants.h"
#include "library/nml/nmlimport.h"
#include "library/nml/nmlpreparation.h"
#include "preferences/beatdetectionsettings.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"
#include "track/beats.h"
#include "track/keyutils.h"
#include "track/serato/tags.h"
#include "track/track.h"

using namespace mixxx::nml;

namespace {

// Seam: applyNmlImport(), observed through the imported Track
// (beats, key, BPM lock) and AnalyzerBeats (re-analysis decision).
class NmlPreparationTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_dir.isValid());
        m_nmlDir = QDir(m_dir.path());
    }

    // Imports `fixture` with the given audio files next to it; returns the
    // tracks in playlist order.
    QList<TrackPointer> importFixture(const QString& fixture, const QStringList& audio) {
        for (const QString& f : audio) {
            nmltest::copyAudio(getTestDir(), f, m_nmlDir);
        }
        const QString nml = nmltest::writeFixture(getTestDir(), fixture, m_nmlDir, "set.nml");
        PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
        const NmlImportPlan plan = planNmlImport(nml, dao);
        EXPECT_TRUE(plan.error.isEmpty());
        const NmlImportResult result =
                applyNmlImport(plan, plan.defaultName, trackCollectionManager());
        EXPECT_TRUE(result.error.isEmpty()) << result.error.toStdString();
        QList<TrackPointer> tracks;
        for (const NmlPlanTrack& item : plan.tracks) {
            tracks.append(trackCollectionManager()->getTrackByRef(
                    TrackRef::fromFilePath(item.resolvedPath)));
        }
        return tracks;
    }

    static double frameOfMillis(const TrackPointer& pTrack, double millis) {
        return millis * pTrack->getSampleRate() / 1000.0;
    }

    QTemporaryDir m_dir;
    QDir m_nmlDir;
};

const QStringList kGridAudio = {"cover-test-vbr.mp3", "cover-test.flac", "cover-test.ogg", "empty.mp3"};

TEST_F(NmlPreparationTest, SixMarkerGridImportsSixTempoSectionsShiftedByMp3DecoderDelay) {
    const auto tracks = importFixture("traktor-v20-grid.nml", kGridAudio);
    const TrackPointer pTrack = tracks.at(0);
    ASSERT_TRUE(pTrack);
    const mixxx::BeatsPointer pBeats = pTrack->getBeats();
    ASSERT_TRUE(pBeats);

    // Independent source of truth: the NML's literal values.
    const double positionsMillis[] = {
            53.759780, 28082.122397, 44083.673033, 79119.455225, 95119.904427, 129718.069594};
    const double bpms[] = {137.004074, 119.988373, 137.002792, 119.996628, 137.001480, 136.971420};

    const double offsetMillis = mixxx::SeratoTags::guessTimingOffsetMillis(
            pTrack->getLocation(),
            pTrack->getType(),
            mixxx::audio::SignalInfo(mixxx::audio::ChannelCount(pTrack->getChannels()),
                    pTrack->getSampleRate()));

    for (int i = 0; i < 6; ++i) {
        const double expectedFrame = frameOfMillis(pTrack, positionsMillis[i] + offsetMillis);
        const auto marker = mixxx::audio::FramePos(expectedFrame);
        // The offset is whatever the Serato import would apply on this platform.
        EXPECT_NEAR(expectedFrame, pBeats->findClosestBeat(marker).value(), 1.0) << i;
        // Inside the section: 8 beats later (the shortest section has 32 beats).
        const auto inside = pBeats->findNthBeat(marker + 1, 8);
        EXPECT_NEAR(bpms[i], pBeats->getBpmAroundPosition(inside, 4).value(), 0.1) << i;
    }
}

TEST_F(NmlPreparationTest, EverySectionTempoSurvivesReloadFromDatabase) {
    auto tracks = importFixture("traktor-v20-grid.nml", kGridAudio);
    const TrackId trackId = tracks.at(0)->getId();
    ASSERT_TRUE(trackId.isValid());
    ASSERT_NE(TrackCollectionManager::SaveTrackResult::Failed,
            trackCollectionManager()->saveTrack(tracks.at(0)));
    tracks.clear(); // drop the cache entry so the next fetch reads the database

    const TrackPointer pTrack = trackCollectionManager()->getTrackById(trackId);
    ASSERT_TRUE(pTrack);
    const mixxx::BeatsPointer pBeats = pTrack->getBeats();
    ASSERT_TRUE(pBeats);
    const double offsetMillis = mixxx::SeratoTags::guessTimingOffsetMillis(
            pTrack->getLocation(),
            pTrack->getType(),
            mixxx::audio::SignalInfo(mixxx::audio::ChannelCount(pTrack->getChannels()),
                    pTrack->getSampleRate()));
    const double positionsMillis[] = {
            53.759780, 28082.122397, 44083.673033, 79119.455225, 95119.904427, 129718.069594};
    const double bpms[] = {137.004074, 119.988373, 137.002792, 119.996628, 137.001480, 136.971420};
    for (int i = 0; i < 6; ++i) {
        const auto marker = mixxx::audio::FramePos(
                frameOfMillis(pTrack, positionsMillis[i] + offsetMillis));
        const auto inside = pBeats->findNthBeat(pBeats->findClosestBeat(marker) + 1, 8);
        EXPECT_NEAR(bpms[i], pBeats->getBpmAroundPosition(inside, 4).value(), 0.015) << i;
    }
}

TEST_F(NmlPreparationTest, SingleMarkerGridIsConstantTempoAtMarkerWithoutOffsetOnLosslessFiles) {
    const auto tracks = importFixture("traktor-v20-grid.nml", kGridAudio);
    const TrackPointer pTrack = tracks.at(1); // flac
    ASSERT_TRUE(pTrack);
    const mixxx::BeatsPointer pBeats = pTrack->getBeats();
    ASSERT_TRUE(pBeats);
    EXPECT_NEAR(frameOfMillis(pTrack, 1836.328431),
            pBeats->findClosestBeat(mixxx::audio::FramePos(frameOfMillis(pTrack, 1836.328431))).value(),
            1.0);
    EXPECT_NEAR(136.0, pBeats->getBpmAroundPosition(pBeats->firstBeat(), 8).value(), 0.001);
    EXPECT_NEAR(136.0, pBeats->getLastMarkerBpm().value(), 0.001);
}

TEST_F(NmlPreparationTest, Version19GridUsesTempoBpm) {
    const auto tracks = importFixture("traktor-v19-playlist.nml",
            {"cover-test.flac", "empty.mp3", "cover-test.ogg"});
    const TrackPointer pTrack = tracks.at(0);
    ASSERT_TRUE(pTrack);
    const mixxx::BeatsPointer pBeats = pTrack->getBeats();
    ASSERT_TRUE(pBeats);
    EXPECT_NEAR(frameOfMillis(pTrack, 137.113678),
            pBeats->findClosestBeat(mixxx::audio::FramePos(frameOfMillis(pTrack, 137.113678))).value(),
            1.0);
    EXPECT_NEAR(151.999603, pBeats->getLastMarkerBpm().value(), 0.0001);
}

TEST_F(NmlPreparationTest, ImportedGridCarriesTraktorSubVersion) {
    const auto tracks = importFixture("traktor-v20-grid.nml", kGridAudio);
    EXPECT_EQ(mixxx::nml::kBeatsSubVersion, tracks.at(1)->getBeats()->getSubVersion());
    EXPECT_EQ(mixxx::nml::kBeatsSubVersion, tracks.at(0)->getBeats()->getSubVersion());
}

TEST_F(NmlPreparationTest, TrackWithoutGridCuesGetsNoGrid) {
    const auto tracks = importFixture("traktor-v20-grid.nml", kGridAudio);
    EXPECT_FALSE(tracks.at(2)->getBeats());
}

TEST_F(NmlPreparationTest, KeysFromMusicalKeyThenKeyText) {
    const auto tracks = importFixture("traktor-v20-grid.nml", kGridAudio);
    using namespace mixxx::track::io::key;
    EXPECT_EQ(F_SHARP_MINOR, tracks.at(0)->getKey()); // MUSICAL_KEY 18
    EXPECT_EQ(G_MAJOR, tracks.at(1)->getKey());       // no MUSICAL_KEY, KEY="2d"
    EXPECT_EQ(C_MAJOR, tracks.at(2)->getKey());       // 0
    EXPECT_EQ(A_MINOR, tracks.at(3)->getKey());       // 21
}

TEST_F(NmlPreparationTest, LockOneLocksBpm) {
    const auto tracks = importFixture("traktor-v20-grid.nml", kGridAudio);
    EXPECT_TRUE(tracks.at(0)->isBpmLocked());
    EXPECT_FALSE(tracks.at(1)->isBpmLocked());
}

TEST(NmlPositionConverterTest, AppliesDecoderDelayBothWays) {
    const mixxx::audio::SignalInfo signalInfo(
            mixxx::audio::ChannelCount(2), mixxx::audio::SampleRate(44100));
    const NmlPositionConverter converter(signalInfo, -26.0);
    // 1000 ms - 26 ms = 974 ms = 42953.4 frames at 44.1 kHz
    EXPECT_NEAR(42953.4, converter.toFrame(1000.0).value(), 0.01);
    EXPECT_NEAR(1000.0, converter.toMillis(converter.toFrame(1000.0)), 0.03);
    EXPECT_NEAR(44100.0, NmlPositionConverter(signalInfo, 0.0).toFrame(1000.0).value(), 1e-6);
}

TEST(NmlPositionConverterTest, GridMarkersAreShiftedByTheTimingOffset) {
    const mixxx::audio::SignalInfo signalInfo(
            mixxx::audio::ChannelCount(2), mixxx::audio::SampleRate(44100));
    NmlEntry entry;
    NmlCue cue;
    cue.type = NmlCueType::Grid;
    cue.startMillis = 1000.0;
    cue.gridBpm = 120.0;
    entry.cues.append(cue);
    const auto pBeats = beatsFromNmlEntry(entry, NmlPositionConverter(signalInfo, -26.0));
    ASSERT_TRUE(pBeats);
    EXPECT_NEAR(42953.4, pBeats->findClosestBeat(mixxx::audio::FramePos(42953.4)).value(), 1.0);
    EXPECT_EQ(kBeatsSubVersion, pBeats->getSubVersion());
}

class NmlAnalyzerReanalysisTest: public NmlPreparationTest {
  protected:
    bool analyzerWouldAnalyze(const TrackPointer& pTrack, bool reanalyzeImported) {
        BeatDetectionSettings(config()).setReanalyzeImported(reanalyzeImported);
        AnalyzerBeats analyzer(config(), true);
        const bool result = analyzer.initialize(AnalyzerTrack(pTrack),
                pTrack->getSampleRate(),
                pTrack->getSampleRate() * 60);
        analyzer.cleanup();
        return result;
    }
};

TEST_F(NmlAnalyzerReanalysisTest, TraktorImportedGridIsReanalyzedOnlyWhenPreferenceIsOn) {
    const auto tracks = importFixture("traktor-v20-grid.nml", kGridAudio);
    const TrackPointer pTrack = tracks.at(1); // unlocked
    ASSERT_TRUE(pTrack->getBeats());
    EXPECT_FALSE(analyzerWouldAnalyze(pTrack, false));
    EXPECT_TRUE(analyzerWouldAnalyze(pTrack, true));
}

} // namespace
