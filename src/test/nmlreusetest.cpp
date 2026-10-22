#include <gtest/gtest.h>

#include <QDir>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlimport.h"
#include "library/nml/nmlimportflow.h"
#include "library/nml/nmlreuse.h"
#include "library/trackcollection.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"
#include "track/beats.h"
#include "track/cue.h"
#include "track/track.h"

using namespace mixxx::nml;

namespace {

// Seam: planNmlImport + proposeNmlReuse + applyNmlImport (ADR 0008). The
// library holds the originals, prepared by importing traktor-v20-cues.nml from
// folder A; the same NML is then imported from folder B, next to copies of
// the audio. Observed through the plan rows, the library size and the result
// lists.
class NmlReuseTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_dirA.isValid());
        ASSERT_TRUE(m_dirB.isValid());
        m_a = QDir(m_dirA.path());
        m_b = QDir(m_dirB.path());
        for (const char* name : {"cover-test-vbr.mp3", "cover-test.flac"}) {
            nmltest::copyAudio(getTestDir(), name, m_a);
            nmltest::copyAudio(getTestDir(), name, m_b);
        }
        PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
        const QString nmlA =
                nmltest::writeFixture(getTestDir(), "traktor-v20-cues.nml", m_a, "set.nml");
        NmlImportPlan first = planNmlImport(nmlA, dao);
        ASSERT_TRUE(first.error.isEmpty());
        ASSERT_EQ(2, applyNmlImport(first, "originals", trackCollectionManager()).newLibraryTracks);
        m_nmlB = nmltest::writeFixture(getTestDir(), "traktor-v20-cues.nml", m_b, "set.nml");
    }

    TrackPointer original(const QString& file) {
        return trackCollectionManager()->getTrackByRef(
                TrackRef::fromFilePath(m_a.filePath(file)));
    }

    NmlImportPlan planB() {
        PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
        NmlImportPlan p = planNmlImport(m_nmlB, dao);
        EXPECT_TRUE(p.error.isEmpty());
        for (const NmlPlanTrack& t : p.tracks) {
            // Fixture PLAYTIME does not match the 30 s test audio.
            const QString file = p.document.entries.at(t.entryIndex).location.file;
            p.document.entries[t.entryIndex].playtimeSeconds = original(file)->getDuration();
        }
        proposeNmlReuse(&p, trackCollectionManager());
        return p;
    }

    int libraryTrackCount() {
        QSqlQuery q(internalCollection()->database());
        EXPECT_TRUE(q.exec("SELECT COUNT(*) FROM library"));
        q.next();
        return q.value(0).toInt();
    }

    static void moveMainCue(const TrackPointer& pTrack, double seconds) {
        const CuePointer pCue = pTrack->findCueByType(mixxx::CueType::MainCue);
        ASSERT_TRUE(pCue);
        pCue->setStartPosition(mixxx::audio::FramePos(
                pCue->getPosition().value() + seconds * pTrack->getSampleRate()));
    }

    QTemporaryDir m_dirA;
    QTemporaryDir m_dirB;
    QDir m_a;
    QDir m_b;
    QString m_nmlB;
};

TEST_F(NmlReuseTest, IdenticalCopiesReuseTheLibraryTracks) {
    NmlImportPlan plan = planB();
    ASSERT_TRUE(nmlRowIsReuse(plan.tracks.at(0)));
    EXPECT_EQ(original("cover-test-vbr.mp3")->getId(), plan.tracks.at(0).reuseTrackId);
    EXPECT_TRUE(plan.tracks.at(0).reuseAccepted);
    const int before = libraryTrackCount();

    const NmlImportResult r = applyNmlImport(plan, "copies", trackCollectionManager());

    EXPECT_EQ(0, r.newLibraryTracks);
    EXPECT_EQ(before, libraryTrackCount());
    EXPECT_EQ(plan.tracks.size(), r.reusedTrackLabels.size());
    EXPECT_TRUE(r.keptSeparateLabels.isEmpty());
    EXPECT_EQ(plan.tracks.size(), r.tracksInPlaylist);
}

TEST_F(NmlReuseTest, ACueMovedBy50msKeepsTheCopySeparate) {
    moveMainCue(original("cover-test-vbr.mp3"), 0.05);

    NmlImportPlan plan = planB();
    EXPECT_FALSE(nmlRowIsReuse(plan.tracks.at(0)));
    EXPECT_TRUE(plan.tracks.at(0).keptSeparate);
    const NmlImportResult r = applyNmlImport(plan, "copies", trackCollectionManager());
    EXPECT_EQ(1, r.newLibraryTracks);
    EXPECT_EQ(1, r.keptSeparateLabels.size());
}

TEST_F(NmlReuseTest, ACueMovedBy5msStillCountsAsIdentical) {
    moveMainCue(original("cover-test-vbr.mp3"), 0.005);
    EXPECT_TRUE(nmlRowIsReuse(planB().tracks.at(0)));
}

TEST_F(NmlReuseTest, AKeyOnOneSideOnlyKeepsTheCopySeparate) {
    original("cover-test-vbr.mp3")->resetKeys();
    const NmlImportPlan plan = planB();
    EXPECT_FALSE(nmlRowIsReuse(plan.tracks.at(0)));
    EXPECT_TRUE(plan.tracks.at(0).keptSeparate);
    EXPECT_TRUE(nmlRowIsReuse(plan.tracks.at(1))); // the other track is untouched
}

TEST_F(NmlReuseTest, ABpmDifferenceOfMoreThan001KeepsTheCopySeparate) {
    const TrackPointer pTrack = original("cover-test-vbr.mp3");
    ASSERT_TRUE(pTrack->trySetBeats(mixxx::Beats::fromConstTempo(pTrack->getSampleRate(),
            mixxx::audio::FramePos(0),
            mixxx::Bpm(pTrack->getBpm() + 0.05),
            QString())));
    EXPECT_FALSE(nmlRowIsReuse(planB().tracks.at(0)));
}

TEST_F(NmlReuseTest, DifferentRatingKeepsTheCopySeparate) {
    original("cover-test-vbr.mp3")->setRating(1);
    EXPECT_FALSE(nmlRowIsReuse(planB().tracks.at(0)));
}

TEST_F(NmlReuseTest, UntickingAReuseRowAddsTheCopyAsItsOwnTrack) {
    NmlImportPlan plan = planB();
    ASSERT_TRUE(nmlRowIsReuse(plan.tracks.at(0)));
    setNmlReuseAccepted(&plan, 0, false);

    const NmlImportResult r = applyNmlImport(plan, "copies", trackCollectionManager());

    EXPECT_EQ(1, r.newLibraryTracks);
    EXPECT_EQ(1, r.reusedTrackLabels.size());
    EXPECT_EQ(1, r.keptSeparateLabels.size());
    EXPECT_TRUE(trackCollectionManager()->getTrackByRef(
            TrackRef::fromFilePath(m_b.filePath("cover-test-vbr.mp3"))));
}

TEST_F(NmlReuseTest, SummaryListsReusedAndKeptSeparateTracks) {
    const QString text = nmlImportSummary(QStringLiteral("Set"), 2, 1, {}, {}, {}, 0, {},
            {QStringLiteral("A - Reused")}, {QStringLiteral("B - Separate")});
    EXPECT_TRUE(text.contains(QStringLiteral("A - Reused")));
    EXPECT_TRUE(text.contains(QStringLiteral("kept separate: preparation differs")));
    EXPECT_TRUE(text.contains(QStringLiteral("B - Separate")));
}

TEST_F(NmlReuseTest, FilesAlreadyInTheLibraryAreNotCopies) {
    trackCollectionManager()->getOrAddTrack(
            TrackRef::fromFilePath(m_b.filePath("cover-test-vbr.mp3")));
    EXPECT_FALSE(nmlRowIsReuse(planB().tracks.at(0)));
}

} // namespace
