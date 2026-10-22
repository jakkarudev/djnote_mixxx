#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "library/dao/playlistdao.h"
#include "library/nml/nmldocument.h"
#include "library/nml/nmlexport.h"
#include "library/nml/nmlexportpreparation.h"
#include "library/nml/nmlimport.h"
#include "library/nml/nmlpreparation.h"
#include "library/nml/nmlwriter.h"
#include "library/trackcollectionmanager.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"
#include "track/beats.h"
#include "track/cue.h"
#include "track/keyutils.h"
#include "track/track.h"
#include "util/color/predefinedcolorpalettes.h"

// DJNote: NML export preparation (#116). Seams: the pure Track -> NmlWriteEntry
// step (nmlWriteEntryFromTrackPreparation and the small mappings), the written
// bytes, and the round trip export -> import into a fresh library.

using namespace mixxx;
using namespace mixxx::nml;

namespace {

constexpr double kFrameToleranceMillis = 1000.0 / 44100.0 * 1.5;

QDateTime exportTime() {
    return QDateTime(QDate(2026, 10, 22), QTime(12, 34, 56), Qt::UTC);
}

const NmlCue* findNmlCue(const QList<NmlCue>& cues, NmlCueType type, int hotcue) {
    for (const NmlCue& cue : cues) {
        if (cue.type == type && cue.hotcue == hotcue) {
            return &cue;
        }
    }
    return nullptr;
}

QList<NmlCue> cuesOfType(const QList<NmlCue>& cues, NmlCueType type) {
    QList<NmlCue> out;
    for (const NmlCue& cue : cues) {
        if (cue.type == type) {
            out.append(cue);
        }
    }
    return out;
}

class NmlExportPreparationTest : public LibraryTest {
  protected:
    TrackPointer newTrack(const QString& audioFile) {
        EXPECT_TRUE(m_dir.isValid());
        const QString path = nmltest::copyAudio(getTestDir(), audioFile, QDir(m_dir.path()));
        TrackPointer pTrack = getOrAddTrackByLocation(path);
        EXPECT_TRUE(pTrack);
        return pTrack;
    }

    NmlWriteEntry exportEntry(const TrackPointer& pTrack) {
        NmlWriteEntry entry;
        fillNmlPreparation(&entry, *pTrack);
        return entry;
    }

    static audio::FramePos frameAtMillis(const TrackPointer& pTrack, double millis) {
        return audio::FramePos(pTrack->getSampleRate() * millis / 1000.0).toLowerFrameBoundary();
    }

    QTemporaryDir m_dir;
};

} // namespace

TEST(NmlExportMappingTest, MusicalKeyIsMixxxKeyMinusOne) {
    EXPECT_EQ(0, nmlMusicalKeyFromKey(track::io::key::C_MAJOR));
    EXPECT_EQ(18, nmlMusicalKeyFromKey(track::io::key::F_SHARP_MINOR));
    EXPECT_EQ(23, nmlMusicalKeyFromKey(track::io::key::B_MINOR));
    EXPECT_FALSE(nmlMusicalKeyFromKey(track::io::key::INVALID).has_value());
}

TEST(NmlExportMappingTest, OffPaletteColourMapsToTheNearestTraktorColour) {
    // Exact palette colours map to their index.
    EXPECT_EQ(1, nmlColorIndexFromTrackColor(RgbColor::optional(RgbColor(0xFA4B35))));
    EXPECT_EQ(7, nmlColorIndexFromTrackColor(RgbColor::optional(RgbColor(0xFE55EA))));
    // A dark green is nearest to Traktor's green (4), a light blue to blue (5).
    EXPECT_EQ(4, nmlColorIndexFromTrackColor(RgbColor::optional(RgbColor(0x10C020))));
    EXPECT_EQ(5, nmlColorIndexFromTrackColor(RgbColor::optional(RgbColor(0x3090F0))));
    EXPECT_FALSE(nmlColorIndexFromTrackColor(RgbColor::nullopt()).has_value());
}

TEST(NmlExportMappingTest, RankingIs51PerStar) {
    EXPECT_FALSE(nmlRankingFromStars(0).has_value());
    EXPECT_EQ(51, nmlRankingFromStars(1));
    EXPECT_EQ(153, nmlRankingFromStars(3));
    EXPECT_EQ(255, nmlRankingFromStars(5));
}

TEST_F(NmlExportPreparationTest, ConstantGridExportsOneMarkerAtTheFirstBeat) {
    const TrackPointer pTrack = newTrack(QStringLiteral("cover-test.flac"));
    const audio::SampleRate sr = pTrack->getSampleRate();
    pTrack->trySetBeats(Beats::fromConstTempo(sr, audio::FramePos(sr * 0.5), Bpm(128.0)));

    const NmlWriteEntry entry = exportEntry(pTrack);
    const QList<NmlCue> grid = cuesOfType(entry.cues, NmlCueType::Grid);
    ASSERT_EQ(1, grid.size());
    EXPECT_EQ(-1, grid.first().hotcue);
    // The marker sits on the track's first beat (a constant grid extends
    // backwards: 500 ms at 128 BPM is also 500 - 468.75 = 31.25 ms).
    EXPECT_NEAR(31.25, grid.first().startMillis, kFrameToleranceMillis);
    ASSERT_TRUE(grid.first().gridBpm.has_value());
    EXPECT_NEAR(128.0, *grid.first().gridBpm, 1e-6);
    ASSERT_TRUE(entry.bpm.has_value());
    EXPECT_NEAR(128.0, *entry.bpm, 1e-6);
}

TEST_F(NmlExportPreparationTest, ThreeSectionGridExportsThreeMarkersWithTheirBpms) {
    const TrackPointer pTrack = newTrack(QStringLiteral("cover-test.flac"));
    const audio::SampleRate sr = pTrack->getSampleRate();
    // 120 BPM for 8 beats (4 s), 150 BPM for 10 beats (4 s), then 90 BPM.
    const std::vector<BeatMarker> markers = {
            BeatMarker(frameAtMillis(pTrack, 1000.0), 8),
            BeatMarker(frameAtMillis(pTrack, 5000.0), 10)};
    pTrack->trySetBeats(Beats::fromBeatMarkers(
            sr, markers, frameAtMillis(pTrack, 9000.0), Bpm(90.0)));

    const NmlWriteEntry entry = exportEntry(pTrack);
    const QList<NmlCue> grid = cuesOfType(entry.cues, NmlCueType::Grid);
    ASSERT_EQ(3, grid.size());
    EXPECT_NEAR(1000.0, grid[0].startMillis, kFrameToleranceMillis);
    EXPECT_NEAR(5000.0, grid[1].startMillis, kFrameToleranceMillis);
    EXPECT_NEAR(9000.0, grid[2].startMillis, kFrameToleranceMillis);
    EXPECT_NEAR(120.0, *grid[0].gridBpm, 0.01);
    EXPECT_NEAR(150.0, *grid[1].gridBpm, 0.01);
    EXPECT_NEAR(90.0, *grid[2].gridBpm, 1e-6);
    for (const NmlCue& cue : grid) {
        EXPECT_EQ(-1, cue.hotcue);
    }
    // TEMPO@BPM is the first marker's (ADR 0010).
    ASSERT_TRUE(entry.bpm.has_value());
    EXPECT_NEAR(*grid[0].gridBpm, *entry.bpm, 1e-6);
}

TEST_F(NmlExportPreparationTest, KeyIsWrittenAsMusicalKeyAndKeyText) {
    const TrackPointer pTrack = newTrack(QStringLiteral("cover-test.flac"));
    pTrack->setKey(track::io::key::F_SHARP_MINOR, track::io::key::USER);
    NmlWriteEntry entry = exportEntry(pTrack);
    ASSERT_TRUE(entry.musicalKey.has_value());
    EXPECT_EQ(18, *entry.musicalKey);
    EXPECT_EQ(KeyUtils::keyToString(track::io::key::F_SHARP_MINOR), entry.keyText);

    pTrack->setKey(track::io::key::C_MAJOR, track::io::key::USER);
    entry = exportEntry(pTrack);
    EXPECT_EQ(0, entry.musicalKey);

    pTrack->resetKeys();
    entry = exportEntry(pTrack);
    EXPECT_FALSE(entry.musicalKey.has_value());
    EXPECT_TRUE(entry.keyText.isEmpty());
}

TEST_F(NmlExportPreparationTest, HotcuesLoopsMainCueAndFadesLandOnTheExpectedTypes) {
    const TrackPointer pTrack = newTrack(QStringLiteral("cover-test.flac"));
    const audio::SampleRate sr = pTrack->getSampleRate();
    auto at = [&](double millis) { return frameAtMillis(pTrack, millis); };

    pTrack->createAndAddCue(CueType::HotCue, 0, at(2000), audio::kInvalidFramePos, RgbColor(0xFF0000));
    pTrack->createAndAddCue(CueType::HotCue, 7, at(9000), audio::kInvalidFramePos, RgbColor(0x00FF00));
    // Hotcue 12 (index 11) is buttonless in Traktor.
    pTrack->createAndAddCue(CueType::HotCue, 11, at(3000), audio::kInvalidFramePos, RgbColor(0x0000FF));
    pTrack->createAndAddCue(CueType::Loop, 3, at(4000), at(6000), RgbColor(0xFFFF00));
    pTrack->createAndAddCue(CueType::Loop, 9, at(7000), at(7500), RgbColor(0xFFFF00));
    pTrack->setMainCuePosition(at(1000));
    pTrack->createAndAddCue(CueType::Intro, Cue::kNoHotCue, at(500), at(900), RgbColor(0));
    pTrack->createAndAddCue(CueType::Outro, Cue::kNoHotCue, at(20000), at(25000), RgbColor(0));
    ASSERT_TRUE(sr.isValid());

    // Label on the first hotcue.
    for (const CuePointer& pCue : pTrack->getCuePoints()) {
        if (pCue->getType() == CueType::HotCue && pCue->getHotCue() == 0) {
            pCue->setLabel(QStringLiteral("Drop"));
        }
    }

    const NmlWriteEntry entry = exportEntry(pTrack);

    const NmlCue* hot0 = findNmlCue(entry.cues, NmlCueType::Cue, 0);
    ASSERT_NE(nullptr, hot0);
    EXPECT_EQ(QStringLiteral("Drop"), hot0->name);
    EXPECT_NEAR(2000.0, hot0->startMillis, kFrameToleranceMillis);
    ASSERT_TRUE(hot0->color.has_value());
    EXPECT_EQ(0xFF0000, *hot0->color);

    const NmlCue* hot7 = findNmlCue(entry.cues, NmlCueType::Cue, 7);
    ASSERT_NE(nullptr, hot7);
    EXPECT_EQ(QStringLiteral("n.n."), hot7->name);

    const NmlCue* hot12 = findNmlCue(entry.cues, NmlCueType::Cue, -1);
    ASSERT_NE(nullptr, hot12);
    EXPECT_NEAR(3000.0, hot12->startMillis, kFrameToleranceMillis);

    const NmlCue* loop4 = findNmlCue(entry.cues, NmlCueType::Loop, 3);
    ASSERT_NE(nullptr, loop4);
    EXPECT_NEAR(4000.0, loop4->startMillis, kFrameToleranceMillis);
    EXPECT_NEAR(2000.0, loop4->lengthMillis, 2 * kFrameToleranceMillis);

    const NmlCue* loopHigh = findNmlCue(entry.cues, NmlCueType::Loop, -1);
    ASSERT_NE(nullptr, loopHigh);
    EXPECT_NEAR(500.0, loopHigh->lengthMillis, 2 * kFrameToleranceMillis);

    const NmlCue* load = findNmlCue(entry.cues, NmlCueType::Load, -1);
    ASSERT_NE(nullptr, load);
    EXPECT_NEAR(1000.0, load->startMillis, kFrameToleranceMillis);

    const NmlCue* fadeIn = findNmlCue(entry.cues, NmlCueType::FadeIn, -1);
    ASSERT_NE(nullptr, fadeIn);
    EXPECT_NEAR(500.0, fadeIn->startMillis, kFrameToleranceMillis);
    const NmlCue* fadeOut = findNmlCue(entry.cues, NmlCueType::FadeOut, -1);
    ASSERT_NE(nullptr, fadeOut);
    EXPECT_NEAR(25000.0, fadeOut->startMillis, kFrameToleranceMillis);

    // Cues are written in ascending START order.
    for (int i = 1; i < entry.cues.size(); ++i) {
        EXPECT_LE(entry.cues[i - 1].startMillis, entry.cues[i].startMillis);
    }
}

TEST_F(NmlExportPreparationTest, ColourRatingLockAndPlayCountAreExported) {
    const TrackPointer pTrack = newTrack(QStringLiteral("cover-test.flac"));
    pTrack->setColor(RgbColor::optional(RgbColor(0x10C020)));
    pTrack->setRating(4);
    pTrack->trySetBeats(Beats::fromConstTempo(
            pTrack->getSampleRate(), audio::FramePos(0), Bpm(120.0)));
    pTrack->setBpmLocked(true);
    PlayCounter playCounter(5);
    playCounter.setLastPlayedAt(QDateTime(QDate(2026, 4, 29), QTime(10, 0), Qt::UTC));
    pTrack->setPlayCounter(playCounter);

    const NmlWriteEntry entry = exportEntry(pTrack);
    EXPECT_EQ(4, entry.color);
    EXPECT_EQ(204, entry.ranking);
    EXPECT_TRUE(entry.locked);
    EXPECT_EQ(5, entry.playCount);
    EXPECT_EQ(QDate(2026, 4, 29), entry.lastPlayed);

    pTrack->setBpmLocked(false);
    pTrack->setRating(0);
    pTrack->setColor(RgbColor::nullopt());
    const NmlWriteEntry plain = exportEntry(pTrack);
    EXPECT_FALSE(plain.locked);
    EXPECT_FALSE(plain.ranking.has_value());
    EXPECT_FALSE(plain.color.has_value());
}

TEST(NmlExportPreparationWriterTest, InfoAttributesFollowTraktor402Order) {
    // Order taken from Traktor Pro 4.0.2's collection.nml and test.nml:
    // BITRATE GENRE LABEL COMMENT KEY PLAYCOUNT PLAYTIME PLAYTIME_FLOAT RANKING
    // IMPORT_DATE LAST_PLAYED RELEASE_DATE FLAGS.
    NmlWriteEntry e;
    e.path = QStringLiteral("C:/a/x.mp3");
    e.title = QStringLiteral("X");
    e.bitrateBitsPerSecond = 320000;
    e.genre = QStringLiteral("Trance");
    e.label = QStringLiteral("L");
    e.comment = QStringLiteral("C");
    e.keyText = QStringLiteral("6m");
    e.playCount = 2;
    e.playtimeSeconds = 137.11673;
    e.ranking = 204;
    e.color = 4;
    e.lastPlayed = QDate(2026, 4, 29);
    e.releaseDate = QStringLiteral("2021-04-16");
    NmlWriteDocument doc;
    doc.entries = {e};
    doc.playlistOrder = {0};
    doc.exportTimeUtc = exportTime();
    const QString text = QString::fromUtf8(writeNml(doc));
    EXPECT_TRUE(text.contains(QStringLiteral(
            "<INFO BITRATE=\"320000\" GENRE=\"Trance\" LABEL=\"L\" COMMENT=\"C\" KEY=\"6m\" "
            "PLAYCOUNT=\"2\" PLAYTIME=\"138\" PLAYTIME_FLOAT=\"137.116730\" RANKING=\"204\" "
            "COLOR=\"4\" IMPORT_DATE=\"2026/10/22\" LAST_PLAYED=\"2026/4/29\" "
            "RELEASE_DATE=\"2021/4/16\" FLAGS=\"12\"></INFO>")))
            << text.toStdString();
}

