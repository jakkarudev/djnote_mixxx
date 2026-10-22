#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>
#include <cmath>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlcues.h"
#include "library/nml/nmldocument.h"
#include "library/nml/nmlexport.h"
#include "library/nml/nmlexportpreparation.h"
#include "library/nml/nmlimport.h"
#include "library/nml/nmlpreparation.h"
#include "library/trackcollectionmanager.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"
#include "track/beats.h"
#include "track/cue.h"
#include "track/track.h"
#include "util/color/predefinedcolorpalettes.h"

// DJNote: NML export -> import round trip (#116). The export copies the files,
// so the import creates NEW library rows for the copies: nothing of the
// original's state can leak into the result. Seams: nmlExportItemsForPlaylist +
// NmlExportWorker on one side, planNmlImport/applyNmlImport on the other.

using namespace mixxx;
using namespace mixxx::nml;

namespace {

constexpr double kFrameToleranceMillis = 1.5 * 1000.0 / 44100.0;

class NmlRoundTripTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_sources.isValid());
        ASSERT_TRUE(m_exports.isValid());
    }

    TrackPointer addTrack(const QString& audioFile) {
        const QString path = nmltest::copyAudio(getTestDir(), audioFile, QDir(m_sources.path()));
        TrackPointer pTrack = getOrAddTrackByLocation(path);
        EXPECT_TRUE(pTrack);
        return pTrack;
    }

    static audio::FramePos frameAtMillis(const TrackPointer& pTrack, double millis) {
        return audio::FramePos(pTrack->getSampleRate() * millis / 1000.0).toLowerFrameBoundary();
    }

    // Exports `tracks` as a Playlist and returns the imported copies in order.
    QList<TrackPointer> roundTrip(const QList<TrackPointer>& tracks) {
        PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
        const int playlistId = dao.createPlaylist(QStringLiteral("Preparation"));
        QList<TrackId> ids;
        for (const TrackPointer& pTrack : tracks) {
            ids.append(pTrack->getId());
        }
        EXPECT_TRUE(dao.appendTracksToPlaylist(ids, playlistId));
        NmlExportJob job;
        job.nmlPath = QDir(m_exports.path()).filePath(QStringLiteral("Preparation/Preparation.nml"));
        job.playlistName = QStringLiteral("Preparation");
        job.items = nmlExportItemsForPlaylist(trackCollectionManager(), playlistId);
        job.exportTimeUtc = QDateTime(QDate(2026, 10, 22), QTime(12, 0), Qt::UTC);
        NmlExportWorker worker(job);
        worker.run();
        EXPECT_TRUE(worker.result().error.isEmpty());

        const NmlImportPlan plan = planNmlImport(job.nmlPath, dao);
        EXPECT_TRUE(plan.error.isEmpty()) << plan.error.toStdString();
        const NmlImportResult result =
                applyNmlImport(plan, QStringLiteral("Reimported"), trackCollectionManager());
        EXPECT_TRUE(result.error.isEmpty());
        QList<TrackPointer> back;
        for (const NmlPlanTrack& item : plan.tracks) {
            back.append(trackCollectionManager()->getTrackByRef(
                    TrackRef::fromFilePath(item.resolvedPath)));
        }
        return back;
    }

    void prepare(const TrackPointer& pTrack) {
        const audio::SampleRate sr = pTrack->getSampleRate();
        const std::vector<BeatMarker> markers = {
                BeatMarker(frameAtMillis(pTrack, 1000.0), 8),
                BeatMarker(frameAtMillis(pTrack, 5000.0), 10)};
        pTrack->trySetBeats(Beats::fromBeatMarkers(
                sr, markers, frameAtMillis(pTrack, 9000.0), Bpm(90.0)));
        pTrack->setKey(track::io::key::F_SHARP_MINOR, track::io::key::USER);
        pTrack->setBpmLocked(true);
        pTrack->setRating(3);
        pTrack->setColor(PredefinedColorPalettes::kTraktorProTrackColorPalette.at(3));
        pTrack->createAndAddCue(CueType::HotCue, 0, frameAtMillis(pTrack, 2000), audio::kInvalidFramePos, RgbColor(0xFA4B35));
        pTrack->createAndAddCue(CueType::HotCue, 5, frameAtMillis(pTrack, 12000), audio::kInvalidFramePos, RgbColor(0x0187FF));
        pTrack->createAndAddCue(CueType::Loop, 7, frameAtMillis(pTrack, 4000), frameAtMillis(pTrack, 6000), RgbColor(0xFFF700));
        pTrack->setMainCuePosition(frameAtMillis(pTrack, 1000));
        for (const CuePointer& pCue : pTrack->getCuePoints()) {
            if (pCue->getType() == CueType::HotCue && pCue->getHotCue() == 0) {
                pCue->setLabel(QStringLiteral("Drop"));
            }
        }
    }

    static void expectSameGrid(const TrackPointer& a, const TrackPointer& b) {
        const BeatsPointer ba = a->getBeats();
        const BeatsPointer bb = b->getBeats();
        ASSERT_TRUE(ba && bb);
        audio::FramePos pos = ba->findNextBeat(audio::FramePos(0));
        ASSERT_TRUE(pos.isValid());
        // Mixxx persists a non-constant grid as beat positions up to the last
        // marker only, so the tempo AFTER the last marker is not restored when
        // a track is re-read from the database (upstream behaviour, also for
        // the Serato/Rekordbox imports). The last-section tempo is checked in
        // the export tests instead; here we compare up to the last marker.
        const bool constant = ba->hasConstantTempo() && bb->hasConstantTempo();
        const audio::FramePos lastMarker = constant
                ? ba->findNthBeat(pos, 60)
                : std::min(ba->getLastMarkerPosition(), bb->getLastMarkerPosition());
        int compared = 0;
        while (pos.isValid() && pos <= lastMarker) {
            const audio::FramePos other = bb->findClosestBeat(pos);
            ASSERT_TRUE(other.isValid());
            ASSERT_LE(std::abs(other.value() - pos.value()), 1.5)
                    << "beat " << compared << " original " << pos.value() << " imported "
                    << other.value();
            pos = ba->findNextBeat(pos + audio::FrameDiff_t(1.0));
            ++compared;
        }
        EXPECT_GT(compared, 0);
    }

    static void expectSamePreparation(const TrackPointer& a, const TrackPointer& b) {
        ASSERT_NE(a->getId(), b->getId());
        expectSameGrid(a, b);
        EXPECT_EQ(a->getKey(), b->getKey());
        EXPECT_EQ(a->isBpmLocked(), b->isBpmLocked());
        EXPECT_EQ(a->getRating(), b->getRating());
        EXPECT_EQ(a->getColor(), b->getColor());
        EXPECT_NEAR(a->getMainCuePosition().value(), b->getMainCuePosition().value(), 1.5);
        int compared = 0;
        for (const CuePointer& pa : a->getCuePoints()) {
            if (pa->getType() != CueType::HotCue && pa->getType() != CueType::Loop) {
                continue;
            }
            CuePointer match;
            for (const CuePointer& pb : b->getCuePoints()) {
                if (pb->getType() == pa->getType() && pb->getHotCue() == pa->getHotCue()) {
                    match = pb;
                }
            }
            ASSERT_TRUE(match) << "hotcue " << pa->getHotCue();
            ++compared;
            EXPECT_NEAR(pa->getPosition().value(), match->getPosition().value(), 1.5);
            EXPECT_EQ(pa->getLabel(), match->getLabel());
            EXPECT_EQ(static_cast<int>(pa->getColor()), static_cast<int>(match->getColor()));
            if (pa->getType() == CueType::Loop) {
                EXPECT_NEAR(pa->getEndPosition().value(), match->getEndPosition().value(), 1.5);
            }
        }
        EXPECT_EQ(3, compared);
    }

    QTemporaryDir m_sources;
    QTemporaryDir m_exports;
};

} // namespace

TEST_F(NmlRoundTripTest, PreparedFlexibleGridTrackComesBackIdentical) {
    const TrackPointer pTrack = addTrack(QStringLiteral("cover-test.flac"));
    prepare(pTrack);
    const QList<TrackPointer> back = roundTrip({pTrack});
    ASSERT_EQ(1, back.size());
    ASSERT_TRUE(back.first());
    expectSamePreparation(pTrack, back.first());
}

TEST_F(NmlRoundTripTest, Mp3WithDecoderDelayComesBackIdentical) {
    const TrackPointer pTrack = addTrack(QStringLiteral("cover-test-vbr.mp3"));
    prepare(pTrack);
    const QList<TrackPointer> back = roundTrip({pTrack});
    ASSERT_EQ(1, back.size());
    ASSERT_TRUE(back.first());
    expectSamePreparation(pTrack, back.first());
}

TEST_F(NmlRoundTripTest, ConstantGridComesBackWithOneMarker) {
    const TrackPointer pTrack = addTrack(QStringLiteral("cover-test.ogg"));
    pTrack->trySetBeats(Beats::fromConstTempo(
            pTrack->getSampleRate(), frameAtMillis(pTrack, 300), Bpm(128.0)));
    const QList<TrackPointer> back = roundTrip({pTrack});
    ASSERT_EQ(1, back.size());
    ASSERT_TRUE(back.first());
    expectSameGrid(pTrack, back.first());
    EXPECT_TRUE(back.first()->getBeats()->hasConstantTempo());
    EXPECT_NEAR(128.0, back.first()->getBpm(), 0.001);
}

// The platform's own decoder-delay guess is 0 for mp3 on some builds, so the
// -26 ms delay is checked with an explicit converter: export subtracts it,
// import adds it back.
TEST_F(NmlRoundTripTest, DecoderDelayIsSubtractedOnExportAndAddedBackOnImport) {
    const TrackPointer pTrack = addTrack(QStringLiteral("cover-test-vbr.mp3"));
    const NmlPositionConverter converter(
            audio::SignalInfo(audio::ChannelCount(2), pTrack->getSampleRate()), -26.0);
    pTrack->createAndAddCue(CueType::HotCue,
            1,
            frameAtMillis(pTrack, 2000),
            audio::kInvalidFramePos,
            RgbColor(0xFA4B35));
    pTrack->trySetBeats(Beats::fromConstTempo(
            pTrack->getSampleRate(), frameAtMillis(pTrack, 500), Bpm(125.0)));

    NmlWriteEntry written;
    fillNmlPreparation(&written, *pTrack, converter);
    NmlEntry read;
    read.cues = written.cues;
    read.bpm = written.bpm;
    bool sawHotcue = false;
    for (const NmlCue& cue : read.cues) {
        if (cue.type == NmlCueType::Cue) {
            sawHotcue = true;
            EXPECT_NEAR(2026.0, cue.startMillis, kFrameToleranceMillis);
        }
    }
    EXPECT_TRUE(sawHotcue);

    const NmlCueImport imported = cueInfosFromNmlEntry(read, converter);
    ASSERT_EQ(1, imported.cues.size());
    EXPECT_NEAR(2000.0, *imported.cues.first().getStartPositionMillis(), kFrameToleranceMillis);
    const BeatsPointer pBeats = beatsFromNmlEntry(read, converter);
    ASSERT_TRUE(pBeats);
    const audio::FramePos expected = frameAtMillis(pTrack, 500);
    EXPECT_NEAR(expected.value(), pBeats->findClosestBeat(expected).value(), 1.5);
}
