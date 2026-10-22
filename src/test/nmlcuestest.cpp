#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlcues.h"
#include "library/nml/nmlimport.h"
#include "library/nml/nmlimportflow.h"
#include "library/nml/nmlpreparation.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"
#include "track/cue.h"
#include "track/serato/tags.h"
#include "track/track.h"
#include "util/color/predefinedcolorpalettes.h"

using namespace mixxx::nml;

namespace {

// Seam: applyNmlImport(), observed through the imported Track (cues, main
// cue, intro/outro, colour, rating), plus the pure entry -> CueInfo step.
class NmlCuesTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_dir.isValid());
        m_nmlDir = QDir(m_dir.path());
    }

    QList<TrackPointer> importFixture(const QString& fixture, const QStringList& audio) {
        for (const QString& f : audio) {
            nmltest::copyAudio(getTestDir(), f, m_nmlDir);
        }
        const QString nml = nmltest::writeFixture(getTestDir(), fixture, m_nmlDir, "set.nml");
        PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
        const NmlImportPlan plan = planNmlImport(nml, dao);
        EXPECT_TRUE(plan.error.isEmpty());
        m_result = applyNmlImport(plan, plan.defaultName, trackCollectionManager());
        EXPECT_TRUE(m_result.error.isEmpty()) << m_result.error.toStdString();
        QList<TrackPointer> tracks;
        for (const NmlPlanTrack& item : plan.tracks) {
            tracks.append(trackCollectionManager()->getTrackByRef(
                    TrackRef::fromFilePath(item.resolvedPath)));
        }
        return tracks;
    }

    // Frames of an NML position on `pTrack`, from the NML's literal millis.
    static double expectedFrame(const TrackPointer& pTrack, double nmlMillis) {
        const double offsetMillis = mixxx::SeratoTags::guessTimingOffsetMillis(
                pTrack->getLocation(),
                pTrack->getType(),
                mixxx::audio::SignalInfo(mixxx::audio::ChannelCount(pTrack->getChannels()),
                        pTrack->getSampleRate()));
        return (nmlMillis + offsetMillis) * pTrack->getSampleRate() / 1000.0;
    }

    static CuePointer findCue(const TrackPointer& pTrack, mixxx::CueType type, int hotcue) {
        for (const CuePointer& pCue : pTrack->getCuePoints()) {
            if (pCue->getType() == type && pCue->getHotCue() == hotcue) {
                return pCue;
            }
        }
        return CuePointer();
    }

    QTemporaryDir m_dir;
    QDir m_nmlDir;
    NmlImportResult m_result;
};

const QStringList kCueAudio = {"cover-test-vbr.mp3", "cover-test.flac"};

TEST_F(NmlCuesTest, Version20HotcuesLandOnTheirButtonsWithLabelAndColour) {
    const auto tracks = importFixture("traktor-v20-cues.nml", kCueAudio);
    const TrackPointer pTrack = tracks.at(0);
    ASSERT_TRUE(pTrack);

    // NML HOTCUE 0 -> button 1 (index 0): "n.n." is no label, colour #FFFFFF.
    const CuePointer pFirst = findCue(pTrack, mixxx::CueType::HotCue, 0);
    ASSERT_TRUE(pFirst);
    EXPECT_NEAR(expectedFrame(pTrack, 53.759780), pFirst->getPosition().value(), 1.0);
    EXPECT_TRUE(pFirst->getLabel().isEmpty());
    EXPECT_EQ(mixxx::RgbColor(0xFFFFFF), pFirst->getColor());

    const CuePointer pDrop = findCue(pTrack, mixxx::CueType::HotCue, 1);
    ASSERT_TRUE(pDrop);
    EXPECT_NEAR(expectedFrame(pTrack, 28082.122397), pDrop->getPosition().value(), 1.0);
    EXPECT_EQ(QStringLiteral("Drop"), pDrop->getLabel());
    EXPECT_EQ(mixxx::RgbColor(0x0187FF), pDrop->getColor());

    const CuePointer pThird = findCue(pTrack, mixxx::CueType::HotCue, 2);
    ASSERT_TRUE(pThird);
    EXPECT_EQ(mixxx::RgbColor(0x00F329), pThird->getColor());
}

TEST_F(NmlCuesTest, Version20HotcueOneStaysWhereTraktorPutItNotAtTheGrid) {
    const auto tracks = importFixture("traktor-v20-cues.nml", kCueAudio);
    const TrackPointer pBumpy = tracks.at(1);
    ASSERT_TRUE(pBumpy);
    const CuePointer pCue = findCue(pBumpy, mixxx::CueType::HotCue, 0);
    ASSERT_TRUE(pCue);
    EXPECT_NEAR(expectedFrame(pBumpy, 71.622549), pCue->getPosition().value(), 1.0);
    // The grid marker (1836.3 ms) never becomes a hotcue by itself.
    int hotcues = 0;
    for (const CuePointer& c : pBumpy->getCuePoints()) {
        hotcues += c->getType() == mixxx::CueType::HotCue ? 1 : 0;
    }
    EXPECT_EQ(1, hotcues);
}

TEST_F(NmlCuesTest, LoopOnAButtonBecomesALoopWithStartAndEnd) {
    const auto tracks = importFixture("traktor-v20-cues.nml", kCueAudio);
    const CuePointer pLoop = findCue(tracks.at(0), mixxx::CueType::Loop, 6);
    ASSERT_TRUE(pLoop);
    EXPECT_NEAR(expectedFrame(tracks.at(0), 106739.366592), pLoop->getPosition().value(), 1.0);
    EXPECT_NEAR(expectedFrame(tracks.at(0), 106739.366592 + 7111.102262),
            pLoop->getEndPosition().value(),
            1.0);
}

TEST_F(NmlCuesTest, LoadMarkerBecomesTheMainCueAndFadesBecomeIntroAndOutro) {
    const auto tracks = importFixture("traktor-v20-cues.nml", kCueAudio);
    const TrackPointer pTrack = tracks.at(0);
    EXPECT_NEAR(expectedFrame(pTrack, 79119.455225),
            pTrack->getMainCuePosition().value(),
            1.0);
    const CuePointer pIntro = pTrack->findCueByType(mixxx::CueType::Intro);
    ASSERT_TRUE(pIntro);
    EXPECT_NEAR(expectedFrame(pTrack, 95119.904427), pIntro->getPosition().value(), 1.0);
    const CuePointer pOutro = pTrack->findCueByType(mixxx::CueType::Outro);
    ASSERT_TRUE(pOutro);
    EXPECT_NEAR(expectedFrame(pTrack, 129718.069594), pOutro->getEndPosition().value(), 1.0);
}

TEST_F(NmlCuesTest, WithoutLoadMarkerTheFirstButtonlessCueIsTheMainCue) {
    const auto tracks = importFixture("traktor-v20-cues.nml", kCueAudio);
    const TrackPointer pBumpy = tracks.at(1);
    EXPECT_NEAR(expectedFrame(pBumpy, 70659.857843),
            pBumpy->getMainCuePosition().value(),
            1.0);
}

TEST_F(NmlCuesTest, ExtraButtonlessCuesAndLoopsAreDroppedAndCounted) {
    importFixture("traktor-v20-cues.nml", kCueAudio);
    // Connected: a buttonless cue and a buttonless loop beyond the Load marker.
    // Bumpy: a second buttonless cue.
    EXPECT_EQ(3, m_result.droppedCues);
    const QString text = nmlImportSummary(QStringLiteral("Set"), 2, 2, {}, {}, {}, 3);
    EXPECT_TRUE(text.contains(QStringLiteral("3")));
    EXPECT_TRUE(text.contains(QStringLiteral("cue"), Qt::CaseInsensitive));
    EXPECT_FALSE(nmlImportSummary(QStringLiteral("Set"), 2, 2, {})
                         .contains(QStringLiteral("cue"), Qt::CaseInsensitive));
}

TEST_F(NmlCuesTest, ColourIndexFourIsTheTraktorPaletteGreenAndRankingIsInStars) {
    const auto tracks = importFixture("traktor-v20-cues.nml", kCueAudio);
    ASSERT_TRUE(tracks.at(0)->getColor().has_value());
    EXPECT_EQ(mixxx::RgbColor(0x00F329), *tracks.at(0)->getColor());
    EXPECT_EQ(4, tracks.at(0)->getRating()); // RANKING 204 / 51
    // The second entry has neither.
    EXPECT_FALSE(tracks.at(1)->getColor().has_value());
    EXPECT_EQ(0, tracks.at(1)->getRating());
}

TEST_F(NmlCuesTest, Version19GridMarkerOnButtonOneAlsoCreatesHotcueOneNamedAutoGrid) {
    const auto tracks = importFixture("traktor-v19-playlist.nml",
            {"cover-test.flac", "empty.mp3", "cover-test.ogg"});
    const TrackPointer pTrack = tracks.at(0);
    ASSERT_TRUE(pTrack);
    const CuePointer pGridCue = findCue(pTrack, mixxx::CueType::HotCue, 0);
    ASSERT_TRUE(pGridCue);
    EXPECT_NEAR(expectedFrame(pTrack, 137.113678), pGridCue->getPosition().value(), 1.0);
    EXPECT_EQ(QStringLiteral("AutoGrid"), pGridCue->getLabel());
    // Its other hotcue keeps its button.
    const CuePointer pOther = findCue(pTrack, mixxx::CueType::HotCue, 7);
    ASSERT_TRUE(pOther);
    EXPECT_NEAR(expectedFrame(pTrack, 18961.292427), pOther->getPosition().value(), 1.0);
}

TEST(NmlCueInfosTest, PositionsAreShiftedByTheDecoderDelay) {
    NmlEntry entry;
    NmlCue cue;
    cue.type = NmlCueType::Cue;
    cue.hotcue = 3;
    cue.startMillis = 1000.0;
    cue.name = QStringLiteral("n.n.");
    entry.cues.append(cue);
    NmlCue load;
    load.type = NmlCueType::Load;
    load.startMillis = 2000.0;
    entry.cues.append(load);
    NmlCue loop;
    loop.type = NmlCueType::Loop;
    loop.hotcue = 4;
    loop.startMillis = 3000.0;
    loop.lengthMillis = 500.0;
    entry.cues.append(loop);

    const NmlPositionConverter converter(
            mixxx::audio::SignalInfo(mixxx::audio::ChannelCount(2), mixxx::audio::SampleRate(44100)),
            -26.0);
    const NmlCueImport imported = cueInfosFromNmlEntry(entry, converter);
    EXPECT_EQ(0, imported.dropped);
    ASSERT_EQ(3, imported.cues.size());
    bool sawHotcue = false;
    bool sawMain = false;
    bool sawLoop = false;
    for (const mixxx::CueInfo& info : imported.cues) {
        if (info.getType() == mixxx::CueType::HotCue) {
            sawHotcue = true;
            EXPECT_DOUBLE_EQ(974.0, *info.getStartPositionMillis());
            EXPECT_EQ(3, *info.getHotCueIndex());
        } else if (info.getType() == mixxx::CueType::MainCue) {
            sawMain = true;
            EXPECT_DOUBLE_EQ(1974.0, *info.getStartPositionMillis());
        } else if (info.getType() == mixxx::CueType::Loop) {
            sawLoop = true;
            EXPECT_DOUBLE_EQ(2974.0, *info.getStartPositionMillis());
            EXPECT_DOUBLE_EQ(3474.0, *info.getEndPositionMillis());
        }
    }
    EXPECT_TRUE(sawHotcue && sawMain && sawLoop);
}

TEST(NmlCueInfosTest, RankingAndColourMapping) {
    EXPECT_EQ(4, starsFromNmlRanking(204));
    EXPECT_EQ(0, starsFromNmlRanking(0));
    EXPECT_EQ(5, starsFromNmlRanking(255));
    EXPECT_EQ(1, starsFromNmlRanking(51));
    EXPECT_FALSE(trackColorFromNmlIndex(0).has_value());
    EXPECT_FALSE(trackColorFromNmlIndex(8).has_value());
    EXPECT_EQ(mixxx::RgbColor(0xFA4B35), *trackColorFromNmlIndex(1));
    EXPECT_EQ(mixxx::RgbColor(0xFE55EA), *trackColorFromNmlIndex(7));
}

} // namespace
