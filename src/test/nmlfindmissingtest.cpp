#include <gtest/gtest.h>

#include <QDir>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlfindmissing.h"
#include "library/nml/nmlimport.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"
#include "track/track.h"

using namespace mixxx::nml;

namespace {

class NmlFindMissingTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_nmlTemp.isValid());
        ASSERT_TRUE(m_otherTemp.isValid());
        m_nmlDir = QDir(m_nmlTemp.path());
        m_otherDir = QDir(m_otherTemp.path());
    }

    PlaylistDAO& playlistDao() const {
        return internalCollection()->getPlaylistDAO();
    }

    // The v20 fixture with NO audio next to it: all four rows are NotFound.
    NmlImportPlan planAllMissing() {
        const QString nml = nmltest::writeFixture(getTestDir(),
                QStringLiteral("traktor-v20-playlist.nml"),
                m_nmlDir,
                QStringLiteral("set.nml"));
        NmlImportPlan plan = planNmlImport(nml, playlistDao());
        EXPECT_TRUE(plan.error.isEmpty());
        EXPECT_EQ(4, plan.tracks.size());
        return plan;
    }

    // Gives entry `entryIndex` the playtime of `name`'s real file plus `delta`.
    TrackPointer addLibraryTrack(const QString& audioName, const QString& asName = QString()) {
        const QString src = nmltest::copyAudio(getTestDir(), audioName, m_otherDir);
        QString path = src;
        if (!asName.isEmpty()) {
            path = m_otherDir.filePath(asName);
            EXPECT_TRUE(QFile::rename(src, path));
        }
        TrackPointer pTrack = getOrAddTrackByLocation(path);
        EXPECT_TRUE(pTrack);
        return pTrack;
    }

    static void setPlaytime(NmlImportPlan* pPlan, int row, double seconds) {
        pPlan->document.entries[pPlan->tracks.at(row).entryIndex].playtimeSeconds = seconds;
    }

    QList<TrackId> tracksInOrder(int playlistId) const {
        QSqlQuery query(internalCollection()->database());
        query.prepare(QStringLiteral(
                "SELECT track_id FROM PlaylistTracks WHERE playlist_id = :id ORDER BY position"));
        query.bindValue(":id", playlistId);
        QList<TrackId> ids;
        EXPECT_TRUE(query.exec());
        while (query.next()) {
            ids.append(TrackId(query.value(0)));
        }
        return ids;
    }

    QTemporaryDir m_nmlTemp;
    QTemporaryDir m_otherTemp;
    QDir m_nmlDir;
    QDir m_otherDir;
};

TEST_F(NmlFindMissingTest, BrowseResolvesMissingRowsByFileName) {
    NmlImportPlan plan = planAllMissing();
    // The moved folder holds two of the three files.
    nmltest::copyAudio(getTestDir(), QStringLiteral("cover-test-vbr.mp3"), m_otherDir);
    nmltest::copyAudio(getTestDir(), QStringLiteral("cover-test.ogg"), m_otherDir);

    EXPECT_EQ(3, browseNmlMissingInFolder(&plan, m_otherDir.path())); // vbr appears twice

    EXPECT_EQ(NmlTrackStatus::FoundInBrowsedFolder, plan.tracks.at(0).status);
    EXPECT_EQ(m_otherDir.filePath(QStringLiteral("cover-test-vbr.mp3")),
            plan.tracks.at(0).resolvedPath);
    EXPECT_EQ(NmlTrackStatus::NotFound, plan.tracks.at(1).status); // flac not there
    EXPECT_TRUE(plan.tracks.at(1).resolvedPath.isEmpty());
    EXPECT_EQ(NmlTrackStatus::FoundInBrowsedFolder, plan.tracks.at(2).status);
    EXPECT_EQ(NmlTrackStatus::FoundInBrowsedFolder, plan.tracks.at(3).status);
}

TEST_F(NmlFindMissingTest, BrowseLeavesFoundRowsAlone) {
    nmltest::copyAudio(getTestDir(), QStringLiteral("cover-test.flac"), m_nmlDir);
    NmlImportPlan plan = planAllMissing();
    ASSERT_EQ(NmlTrackStatus::FoundNextToNml, plan.tracks.at(1).status);
    nmltest::copyAudio(getTestDir(), QStringLiteral("cover-test.flac"), m_otherDir);
    EXPECT_EQ(0, browseNmlMissingInFolder(&plan, m_otherDir.path()));
    EXPECT_EQ(NmlTrackStatus::FoundNextToNml, plan.tracks.at(1).status);
}

TEST_F(NmlFindMissingTest, AppliesBrowsedFolderFiles) {
    NmlImportPlan plan = planAllMissing();
    for (const char* f : {"cover-test-vbr.mp3", "cover-test.flac", "cover-test.ogg"}) {
        nmltest::copyAudio(getTestDir(), QString::fromLatin1(f), m_otherDir);
    }
    EXPECT_EQ(4, browseNmlMissingInFolder(&plan, m_otherDir.path()));
    const NmlImportResult result =
            applyNmlImport(plan, plan.defaultName, trackCollectionManager());
    ASSERT_TRUE(result.error.isEmpty());
    EXPECT_EQ(4, result.tracksInPlaylist);
    EXPECT_TRUE(result.missingTracks.isEmpty());
    EXPECT_EQ(3, result.newLibraryTracks);
}

TEST_F(NmlFindMissingTest, ProposesSameFileNameWithinTwoSeconds) {
    NmlImportPlan plan = planAllMissing();
    const TrackPointer pFlac = addLibraryTrack(QStringLiteral("cover-test.flac"));
    const double duration = pFlac->getDuration();
    ASSERT_GT(duration, 20.0);
    setPlaytime(&plan, 1, duration + 1.5);

    EXPECT_EQ(1, findNmlMissingInLibrary(&plan, trackCollectionManager()));
    EXPECT_EQ(NmlTrackStatus::MatchedInLibrary, plan.tracks.at(1).status);
    EXPECT_EQ(pFlac->getId(), plan.tracks.at(1).libraryTrackId);
    EXPECT_TRUE(plan.tracks.at(1).accepted);
}

TEST_F(NmlFindMissingTest, SameFileNameTenSecondsLongerIsNotProposed) {
    NmlImportPlan plan = planAllMissing();
    const TrackPointer pFlac = addLibraryTrack(QStringLiteral("cover-test.flac"));
    setPlaytime(&plan, 1, pFlac->getDuration() + 10.0);

    EXPECT_EQ(0, findNmlMissingInLibrary(&plan, trackCollectionManager()));
    EXPECT_EQ(NmlTrackStatus::NotFound, plan.tracks.at(1).status);
}

TEST_F(NmlFindMissingTest, FallsBackToArtistAndTitle) {
    NmlImportPlan plan = planAllMissing();
    // Same audio under another file name; tags made to match the NML row.
    const TrackPointer pTrack =
            addLibraryTrack(QStringLiteral("cover-test.flac"), QStringLiteral("renamed.flac"));
    pTrack->setArtist(QStringLiteral("bumpin flava"));
    pTrack->setTitle(QStringLiteral("Bumpy"));
    ASSERT_NE(TrackCollectionManager::SaveTrackResult::Failed,
            trackCollectionManager()->saveTrack(pTrack));
    setPlaytime(&plan, 1, pTrack->getDuration() - 1.0);

    EXPECT_EQ(1, findNmlMissingInLibrary(&plan, trackCollectionManager()));
    EXPECT_EQ(NmlTrackStatus::MatchedInLibrary, plan.tracks.at(1).status);
    EXPECT_EQ(pTrack->getId(), plan.tracks.at(1).libraryTrackId);
}

TEST_F(NmlFindMissingTest, ArtistAndTitleStillNeedTheDuration) {
    NmlImportPlan plan = planAllMissing();
    const TrackPointer pTrack =
            addLibraryTrack(QStringLiteral("cover-test.flac"), QStringLiteral("renamed.flac"));
    pTrack->setArtist(QStringLiteral("Bumpin Flava"));
    pTrack->setTitle(QStringLiteral("Bumpy"));
    ASSERT_NE(TrackCollectionManager::SaveTrackResult::Failed,
            trackCollectionManager()->saveTrack(pTrack));
    setPlaytime(&plan, 1, pTrack->getDuration() + 5.0);

    EXPECT_EQ(0, findNmlMissingInLibrary(&plan, trackCollectionManager()));
}

TEST_F(NmlFindMissingTest, UntickedProposalStaysMissingAndTickedJoinsPlaylist) {
    NmlImportPlan plan = planAllMissing();
    const TrackPointer pFlac = addLibraryTrack(QStringLiteral("cover-test.flac"));
    const TrackPointer pOgg = addLibraryTrack(QStringLiteral("cover-test.ogg"));
    setPlaytime(&plan, 1, pFlac->getDuration());
    setPlaytime(&plan, 2, pOgg->getDuration());
    ASSERT_EQ(2, findNmlMissingInLibrary(&plan, trackCollectionManager()));
    setNmlMatchAccepted(&plan, 2, false);
    EXPECT_EQ(NmlTrackStatus::MatchedInLibrary, plan.tracks.at(2).status);

    const NmlImportResult result =
            applyNmlImport(plan, plan.defaultName, trackCollectionManager());
    ASSERT_TRUE(result.error.isEmpty());
    EXPECT_EQ(1, result.matchedLibraryTracks);
    EXPECT_EQ(3, result.missingTracks.size()); // vbr twice, plus the unticked ogg
    const QList<TrackId> ids = tracksInOrder(result.playlistId);
    ASSERT_EQ(4, ids.size());
    EXPECT_EQ(pFlac->getId(), ids.at(1));
    EXPECT_NE(pOgg->getId(), ids.at(2));
    EXPECT_EQ(0, result.newLibraryTracks);
}

TEST_F(NmlFindMissingTest, MissingAndHiddenLibraryTracksAreNotProposed) {
    NmlImportPlan plan = planAllMissing();
    const TrackPointer pFlac = addLibraryTrack(QStringLiteral("cover-test.flac"));
    setPlaytime(&plan, 1, pFlac->getDuration());
    QSqlQuery query(internalCollection()->database());
    ASSERT_TRUE(query.exec(QStringLiteral("UPDATE track_locations SET fs_deleted=1")));
    EXPECT_EQ(0, findNmlMissingInLibrary(&plan, trackCollectionManager()));
}

} // namespace
