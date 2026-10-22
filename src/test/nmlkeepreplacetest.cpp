#include <gtest/gtest.h>

#include <QDir>
#include <QTemporaryDir>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlfindmissing.h"
#include "library/nml/nmlimport.h"
#include "library/nml/nmlkeepreplace.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"
#include "track/beats.h"
#include "track/cue.h"
#include "track/track.h"
#include "util/color/predefinedcolorpalettes.h"

using namespace mixxx::nml;

namespace {

// Seam: applyNmlImport() with a plan whose rows land on tracks the library
// already has (ADR 0008). Observed through the Track (grid, key, cues,
// colour, rating, lock) and the plan's per-part comparison.
// traktor-v20-cues.nml: track 0 = cover-test-vbr.mp3 (grid 137.004, key 21,
// colour 4, rating 4 stars, hotcues 1, 2, 3, loop on button 7, main cue,
// intro, outro); track 1 = cover-test.flac (grid 136, key 16, hotcue 1).
class NmlKeepReplaceTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_dir.isValid());
        m_nmlDir = QDir(m_dir.path());
        nmltest::copyAudio(getTestDir(), "cover-test-vbr.mp3", m_nmlDir);
        nmltest::copyAudio(getTestDir(), "cover-test.flac", m_nmlDir);
        m_nml = nmltest::writeFixture(getTestDir(), "traktor-v20-cues.nml", m_nmlDir, "set.nml");
    }

    TrackPointer libraryTrack(const QString& file) {
        bool already = false;
        const TrackPointer pTrack = trackCollectionManager()->getOrAddTrack(
                TrackRef::fromFilePath(m_nmlDir.filePath(file)), &already);
        EXPECT_TRUE(pTrack);
        return pTrack;
    }

    static void setGrid(const TrackPointer& pTrack, double bpm, int firstFrame) {
        ASSERT_TRUE(pTrack->trySetBeats(mixxx::Beats::fromConstTempo(pTrack->getSampleRate(),
                mixxx::audio::FramePos(firstFrame),
                mixxx::Bpm(bpm),
                QString())));
    }

    static void addHotcue(const TrackPointer& pTrack, int button, double seconds) {
        pTrack->createAndAddCue(mixxx::CueType::HotCue,
                button,
                mixxx::audio::FramePos(seconds * pTrack->getSampleRate()),
                mixxx::audio::kInvalidFramePos);
    }

    static QList<int> hotcueButtons(const TrackPointer& pTrack) {
        QList<int> buttons;
        for (const CuePointer& pCue : pTrack->getCuePoints()) {
            if (pCue->getType() == mixxx::CueType::HotCue) {
                buttons.append(pCue->getHotCue());
            }
        }
        std::sort(buttons.begin(), buttons.end());
        return buttons;
    }

    NmlImportPlan plan(NmlPreparationMode mode) {
        PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
        NmlImportPlan p = planNmlImport(m_nml, dao);
        EXPECT_TRUE(p.error.isEmpty());
        p.preparationMode = mode;
        updateNmlPlanComparisons(&p, trackCollectionManager());
        return p;
    }

    NmlImportResult run(NmlPreparationMode mode) {
        NmlImportPlan p = plan(mode);
        const NmlImportResult r = applyNmlImport(p, p.defaultName, trackCollectionManager());
        EXPECT_TRUE(r.error.isEmpty()) << r.error.toStdString();
        return r;
    }

    void checkLockedGrid(NmlPreparationMode mode) {
        const TrackPointer pTrack = libraryTrack("cover-test-vbr.mp3");
        setGrid(pTrack, 100.0, 0);
        pTrack->setBpmLocked(true);
        pTrack->setRating(1);

        const NmlImportResult r = run(mode);

        EXPECT_DOUBLE_EQ(100.0, pTrack->getBpm());
        ASSERT_EQ(1, r.lockedGridTracks.size());
        EXPECT_NE(-1, r.lockedGridTracks.first().indexOf("Connected"));
        EXPECT_EQ(mode == NmlPreparationMode::Replace ? 4 : 1, pTrack->getRating());
    }

    QTemporaryDir m_dir;
    QDir m_nmlDir;
    QString m_nml;
};

TEST_F(NmlKeepReplaceTest, KeepLeavesDjnoteHotcuesAloneEvenThoughTheNmlHasOthers) {
    const TrackPointer pTrack = libraryTrack("cover-test-vbr.mp3");
    addHotcue(pTrack, 0, 5.0);
    addHotcue(pTrack, 3, 9.0);

    run(NmlPreparationMode::Keep);

    EXPECT_EQ((QList<int>{0, 3}), hotcueButtons(pTrack));
    EXPECT_EQ(nullptr, pTrack->findCueByType(mixxx::CueType::MainCue).get());
    EXPECT_EQ(nullptr, pTrack->findCueByType(mixxx::CueType::Intro).get());
}

TEST_F(NmlKeepReplaceTest, KeepFillsOnlyThePartsTheTrackLacks) {
    const TrackPointer pTrack = libraryTrack("cover-test-vbr.mp3");
    setGrid(pTrack, 100.0, 0); // grid, but no key, colour, rating or cues

    const NmlImportResult r = run(NmlPreparationMode::Keep);

    EXPECT_EQ(100.0, pTrack->getBpm());
    EXPECT_NE(mixxx::track::io::key::INVALID, pTrack->getKey()); // from the NML
    EXPECT_TRUE(pTrack->getColor().has_value());
    EXPECT_EQ(4, pTrack->getRating());
    EXPECT_FALSE(hotcueButtons(pTrack).isEmpty());
    EXPECT_EQ(1, r.existingTracksUpdated);
}

TEST_F(NmlKeepReplaceTest, KeepKeepsAGridButTakesTheKeyOfATrackWithoutOne) {
    const TrackPointer pTrack = libraryTrack("cover-test-vbr.mp3");
    setGrid(pTrack, 100.0, 0);
    addHotcue(pTrack, 5, 3.0);
    ASSERT_EQ(mixxx::track::io::key::INVALID, pTrack->getKey());

    run(NmlPreparationMode::Keep);

    EXPECT_DOUBLE_EQ(100.0, pTrack->getBpm());
    EXPECT_NE(mixxx::track::io::key::INVALID, pTrack->getKey());
    EXPECT_EQ((QList<int>{5}), hotcueButtons(pTrack)); // cues part kept
}

TEST_F(NmlKeepReplaceTest, ReplaceSwapsGridKeyCuesColourAndRating) {
    const TrackPointer pTrack = libraryTrack("cover-test-vbr.mp3");
    setGrid(pTrack, 100.0, 0);
    pTrack->setKey(mixxx::track::io::key::C_MAJOR, mixxx::track::io::key::USER);
    addHotcue(pTrack, 3, 9.0);
    addHotcue(pTrack, 7, 11.0);
    pTrack->setColor(mixxx::RgbColor::optional(mixxx::RgbColor(0x112233)));
    pTrack->setRating(1);

    const NmlImportResult r = run(NmlPreparationMode::Replace);

    EXPECT_NEAR(137.004, pTrack->getBpm(), 0.01);
    EXPECT_NE(mixxx::track::io::key::C_MAJOR, pTrack->getKey());
    EXPECT_EQ(4, pTrack->getRating());
    EXPECT_NE(0x112233, static_cast<int>(*pTrack->getColor()));
    // The whole cues group is swapped: DJNote's buttons 4 and 8 are gone.
    EXPECT_EQ((QList<int>{0, 1, 2}), hotcueButtons(pTrack));
    EXPECT_NE(nullptr, pTrack->findCueByType(mixxx::CueType::MainCue).get());
    EXPECT_EQ(1, r.existingTracksUpdated);
    EXPECT_TRUE(r.lockedGridTracks.isEmpty());
}

TEST_F(NmlKeepReplaceTest, ReplaceKeepsCuesOutsideTheGroup) {
    const TrackPointer pTrack = libraryTrack("cover-test-vbr.mp3");
    pTrack->createAndAddCue(mixxx::CueType::Jump,
            Cue::kNoHotCue,
            mixxx::audio::FramePos(44100),
            mixxx::audio::FramePos(88200));
    addHotcue(pTrack, 6, 2.0);

    run(NmlPreparationMode::Replace);

    EXPECT_NE(nullptr, pTrack->findCueByType(mixxx::CueType::Jump).get());
    EXPECT_FALSE(hotcueButtons(pTrack).contains(6));
}

TEST_F(NmlKeepReplaceTest, BpmLockedGridSurvivesKeepAndIsNamed) {
    checkLockedGrid(NmlPreparationMode::Keep);
}

TEST_F(NmlKeepReplaceTest, BpmLockedGridSurvivesReplaceAndIsNamedWhileOtherPartsReplace) {
    checkLockedGrid(NmlPreparationMode::Replace);
}

TEST_F(NmlKeepReplaceTest, ChoiceOnlyShownWhenATrackHasPreparation) {
    // Not in the library at all, and then in the library without preparation.
    EXPECT_FALSE(nmlPlanHasExistingPreparation(plan(NmlPreparationMode::Keep)));
    const TrackPointer pTrack = libraryTrack("cover-test-vbr.mp3");
    EXPECT_FALSE(nmlPlanHasExistingPreparation(plan(NmlPreparationMode::Keep)));
    EXPECT_TRUE(plan(NmlPreparationMode::Keep).tracks.at(0).existingTrack);

    pTrack->setRating(2);
    EXPECT_TRUE(nmlPlanHasExistingPreparation(plan(NmlPreparationMode::Keep)));
}

TEST_F(NmlKeepReplaceTest, ComparisonNamesTheConflictingParts) {
    const TrackPointer pTrack = libraryTrack("cover-test-vbr.mp3");
    setGrid(pTrack, 100.0, 0);
    pTrack->setRating(4); // equal to the NML's
    addHotcue(pTrack, 0, 5.0);

    const NmlPreparationComparison c = plan(NmlPreparationMode::Keep).tracks.at(0).comparison;
    EXPECT_TRUE(c.grid.conflicts());
    EXPECT_TRUE(c.rating.trackHas && c.rating.nmlCarries && c.rating.same);
    EXPECT_FALSE(c.rating.conflicts());
    EXPECT_TRUE(c.cues.conflicts());
    EXPECT_TRUE(c.key.takenInKeep());
    EXPECT_TRUE(c.color.takenInKeep());
    EXPECT_FALSE(c.identical());
    EXPECT_TRUE(c.anyConflict());
}

TEST_F(NmlKeepReplaceTest, ATrackImportedFromTheSameNmlIsIdentical) {
    run(NmlPreparationMode::Keep); // creates and prepares the tracks
    const NmlImportPlan again = plan(NmlPreparationMode::Keep);
    ASSERT_EQ(2, again.tracks.size());
    for (const NmlPlanTrack& item : again.tracks) {
        EXPECT_TRUE(item.existingTrack);
        EXPECT_TRUE(item.comparison.identical()) << item.title.toStdString();
        EXPECT_FALSE(item.comparison.anyConflict());
    }
}

TEST_F(NmlKeepReplaceTest, AcceptedLibraryMatchGoesThroughTheRule) {
    // The file is elsewhere in the library; the NML row is missing.
    QTemporaryDir other;
    ASSERT_TRUE(other.isValid());
    const QDir otherDir(other.path());
    nmltest::copyAudio(getTestDir(), "cover-test-vbr.mp3", otherDir);
    bool already = false;
    const TrackPointer pTrack = trackCollectionManager()->getOrAddTrack(
            TrackRef::fromFilePath(otherDir.filePath("cover-test-vbr.mp3")), &already);
    ASSERT_TRUE(pTrack);
    addHotcue(pTrack, 4, 6.0);
    pTrack->setRating(1);

    QDir::root().remove(m_nmlDir.filePath("cover-test-vbr.mp3"));
    NmlImportPlan p = plan(NmlPreparationMode::Keep);
    ASSERT_EQ(NmlTrackStatus::NotFound, p.tracks.at(0).status);
    p.document.entries[p.tracks.at(0).entryIndex].playtimeSeconds = pTrack->getDuration();
    ASSERT_EQ(1, findNmlMissingInLibrary(&p, trackCollectionManager()));
    updateNmlPlanComparisons(&p, trackCollectionManager());
    EXPECT_TRUE(p.tracks.at(0).existingTrack);
    EXPECT_TRUE(nmlPlanHasExistingPreparation(p));

    const NmlImportResult r = applyNmlImport(p, p.defaultName, trackCollectionManager());
    ASSERT_TRUE(r.error.isEmpty());
    EXPECT_EQ(1, r.matchedLibraryTracks);
    EXPECT_EQ((QList<int>{4}), hotcueButtons(pTrack)); // cues kept
    EXPECT_EQ(1, pTrack->getRating());                  // rating kept
    EXPECT_NE(mixxx::track::io::key::INVALID, pTrack->getKey()); // key filled in
    EXPECT_NEAR(137.004, pTrack->getBpm(), 0.01);

    // Replace on the same match swaps the cues and rating.
    p.preparationMode = NmlPreparationMode::Replace;
    PlaylistDAO& dao = internalCollection()->getPlaylistDAO();
    applyNmlImport(p, nextFreePlaylistName(dao, "again"), trackCollectionManager());
    EXPECT_EQ((QList<int>{0, 1, 2}), hotcueButtons(pTrack));
    EXPECT_EQ(4, pTrack->getRating());
}

TEST_F(NmlKeepReplaceTest, NewTrackIsNotCountedAsExistingWhenItRepeats) {
    // A fresh import is the normal path; no existing-track update is reported.
    const NmlImportResult r = run(NmlPreparationMode::Replace);
    EXPECT_EQ(0, r.existingTracksUpdated);
    EXPECT_EQ(2, r.newLibraryTracks);
}

} // namespace
