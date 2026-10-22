#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <memory>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlimport.h"
#include "library/nml/nmlreread.h"
#include "library/nml/nmlsourcelink.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"
#include "track/track.h"

using namespace mixxx::nml;

namespace {

// #114: source NML link lifecycle and "Re-read NML" at the import seam
// (planNmlReread / applyNmlReread), against the v20 fixture
// (mp3 "Connected", flac "Bumpy", ogg "A Wee Dub", mp3 repeated).
class NmlRereadTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_dir.isValid());
        m_nmlDir = QDir(m_dir.path());
        m_keeper = std::make_unique<NmlSourceLinkKeeper>(&playlistDao(), database(), nullptr);
    }

    PlaylistDAO& playlistDao() const {
        return internalCollection()->getPlaylistDAO();
    }
    QSqlDatabase database() const {
        return internalCollection()->database();
    }

    QString writeNmlWithAudio(const QStringList& audio) {
        for (const QString& f : audio) {
            nmltest::copyAudio(getTestDir(), f, m_nmlDir);
        }
        return nmltest::writeFixture(getTestDir(),
                QStringLiteral("traktor-v20-playlist.nml"),
                m_nmlDir,
                QStringLiteral("My Set.nml"));
    }

    // Imports the fixture; `without` audio files are left out (missing tracks).
    int importSet(const QStringList& without = {}) {
        QStringList audio = {QStringLiteral("cover-test-vbr.mp3"),
                QStringLiteral("cover-test.flac"),
                QStringLiteral("cover-test.ogg")};
        for (const QString& w : without) {
            audio.removeAll(w);
        }
        m_nml = writeNmlWithAudio(audio);
        const NmlImportPlan plan = planNmlImport(m_nml, playlistDao());
        const NmlImportResult result =
                applyNmlImport(plan, plan.defaultName, trackCollectionManager());
        EXPECT_TRUE(result.error.isEmpty()) << result.error.toStdString();
        return result.playlistId;
    }

    QList<TrackId> tracksInOrder(int playlistId) const {
        QSqlQuery query(database());
        query.prepare(QStringLiteral(
                "SELECT track_id FROM PlaylistTracks WHERE playlist_id = :id "
                "ORDER BY position"));
        query.bindValue(":id", playlistId);
        QList<TrackId> ids;
        EXPECT_TRUE(query.exec());
        while (query.next()) {
            ids.append(TrackId(query.value(0)));
        }
        return ids;
    }

    int libraryRowCount() const {
        QSqlQuery query(database());
        EXPECT_TRUE(query.exec(QStringLiteral("SELECT COUNT(*) FROM library")));
        return query.next() ? query.value(0).toInt() : -1;
    }

    bool isFlaggedMissing(TrackId id) const {
        QSqlQuery query(database());
        query.prepare(QStringLiteral(
                "SELECT track_locations.fs_deleted FROM library "
                "JOIN track_locations ON track_locations.id = library.location "
                "WHERE library.id = :id"));
        query.bindValue(":id", id.toVariant());
        EXPECT_TRUE(query.exec());
        return query.next() && query.value(0).toInt() == 1;
    }

    QTemporaryDir m_dir;
    QDir m_nmlDir;
    QString m_nml;
    std::unique_ptr<NmlSourceLinkKeeper> m_keeper;
};

// ---- the link ----

TEST_F(NmlRereadTest, ImportedPlaylistHasALinkWithPathUuidAndName) {
    const int playlistId = importSet();
    const auto link = loadNmlSourceLink(database(), playlistId);
    ASSERT_TRUE(link.has_value());
    EXPECT_EQ(m_nml, link->nmlPath);
    EXPECT_EQ(QStringLiteral("338613851b2e4bddb2926ac6a11ed148"), link->uuid);
    EXPECT_EQ(QStringLiteral("Preparation"), link->name);
}

TEST_F(NmlRereadTest, RenameKeepsTheLink) {
    const int playlistId = importSet();
    playlistDao().renamePlaylist(playlistId, QStringLiteral("Renamed"));
    EXPECT_TRUE(loadNmlSourceLink(database(), playlistId).has_value());
}

TEST_F(NmlRereadTest, DeleteRemovesTheLink) {
    const int playlistId = importSet();
    playlistDao().deletePlaylist(playlistId);
    EXPECT_FALSE(loadNmlSourceLink(database(), playlistId).has_value());
}

TEST_F(NmlRereadTest, BulkDeleteRemovesTheLink) {
    const int playlistId = importSet();
    playlistDao().deletePlaylists(QStringList{QString::number(playlistId)}); // ids as text
    EXPECT_FALSE(loadNmlSourceLink(database(), playlistId).has_value());
}

TEST_F(NmlRereadTest, ADuplicateHasNoLink) {
    const int playlistId = importSet();
    const int copyId = playlistDao().createPlaylist(QStringLiteral("Copy"));
    ASSERT_NE(kInvalidPlaylistId, copyId);
    playlistDao().copyPlaylistTracks(playlistId, copyId);
    EXPECT_FALSE(loadNmlSourceLink(database(), copyId).has_value());
    EXPECT_TRUE(loadNmlSourceLink(database(), playlistId).has_value());
}

TEST_F(NmlRereadTest, OrdinaryPlaylistHasNoLink) {
    const int playlistId = playlistDao().createPlaylist(QStringLiteral("Plain"));
    EXPECT_FALSE(loadNmlSourceLink(database(), playlistId).has_value());
    EXPECT_FALSE(planNmlReread(playlistId, trackCollectionManager()).error.isEmpty());
}

// ---- Keep mode ----

TEST_F(NmlRereadTest, KeepRelinksAFoundMissingTrackWithItsLibraryIdAndFillsPreparation) {
    const int playlistId = importSet({QStringLiteral("cover-test.flac")});
    const QList<TrackId> before = tracksInOrder(playlistId);
    ASSERT_EQ(4, before.size());
    const TrackId missingId = before.at(1);
    ASSERT_TRUE(isFlaggedMissing(missingId));
    const int rowsBefore = libraryRowCount();

    // The file is back (next to the .nml).
    nmltest::copyAudio(getTestDir(), QStringLiteral("cover-test.flac"), m_nmlDir);
    const NmlRereadPlan plan = planNmlReread(playlistId, trackCollectionManager());
    ASSERT_TRUE(plan.error.isEmpty()) << plan.error.toStdString();
    const NmlRereadResult result =
            applyNmlReread(plan, NmlRereadMode::Keep, trackCollectionManager());
    ASSERT_TRUE(result.error.isEmpty()) << result.error.toStdString();

    EXPECT_EQ(before, tracksInOrder(playlistId)); // list unchanged
    EXPECT_EQ(rowsBefore, libraryRowCount());     // no second track
    EXPECT_FALSE(isFlaggedMissing(missingId));
    ASSERT_EQ(1, result.relinked.size());
    const TrackPointer pTrack = trackCollectionManager()->getTrackById(missingId);
    ASSERT_TRUE(pTrack);
    EXPECT_EQ(m_nmlDir.filePath(QStringLiteral("cover-test.flac")), pTrack->getLocation());
    // The preparation it lacked is filled in (grid and hotcues of "Bumpy").
    EXPECT_TRUE(pTrack->getBeats());
    EXPECT_FALSE(pTrack->getCuePoints().isEmpty());
}

TEST_F(NmlRereadTest, KeepLeavesTheListAloneAndStillMissingTracksStayMissing) {
    const int playlistId = importSet({QStringLiteral("cover-test.flac")});
    // DJNote-side edit: drop the last track.
    playlistDao().removeTracksFromPlaylist(playlistId, QList<int>{4});
    const QList<TrackId> before = tracksInOrder(playlistId);
    ASSERT_EQ(3, before.size());
    const NmlRereadPlan plan = planNmlReread(playlistId, trackCollectionManager());
    const NmlRereadResult result =
            applyNmlReread(plan, NmlRereadMode::Keep, trackCollectionManager());
    ASSERT_TRUE(result.error.isEmpty());
    EXPECT_EQ(before, tracksInOrder(playlistId));
    EXPECT_TRUE(isFlaggedMissing(before.at(1)));
    EXPECT_TRUE(result.relinked.isEmpty());
}

// ---- Replace mode ----

TEST_F(NmlRereadTest, ReplaceRevertsDJNoteEditsAndCountsAddedAndRemoved) {
    const int playlistId = importSet();
    const QList<TrackId> imported = tracksInOrder(playlistId);
    ASSERT_EQ(4, imported.size());
    // DJNote-side edits: remove two entries, add a foreign track.
    playlistDao().removeTracksFromPlaylist(playlistId, QList<int>{2, 3});
    const TrackPointer pOther = getOrAddTrackByLocation(
            getTestDir().filePath(QStringLiteral("id3-test-data/empty.mp3")));
    ASSERT_TRUE(pOther);
    playlistDao().appendTrackToPlaylist(pOther->getId(), playlistId);

    const NmlRereadPlan plan = planNmlReread(playlistId, trackCollectionManager());
    ASSERT_TRUE(plan.error.isEmpty()) << plan.error.toStdString();
    const NmlRereadCounts counts = nmlRereadCounts(plan, trackCollectionManager());
    EXPECT_EQ(2, counts.added);
    EXPECT_EQ(1, counts.removed);

    const NmlRereadResult result =
            applyNmlReread(plan, NmlRereadMode::Replace, trackCollectionManager());
    ASSERT_TRUE(result.error.isEmpty()) << result.error.toStdString();
    EXPECT_EQ(2, result.added);
    EXPECT_EQ(1, result.removed);
    EXPECT_EQ(imported, tracksInOrder(playlistId)); // same library ids, same order
}

TEST_F(NmlRereadTest, ReplaceRelinksAMissingTrackInsteadOfAddingASecondOne) {
    const int playlistId = importSet({QStringLiteral("cover-test.ogg")});
    const QList<TrackId> imported = tracksInOrder(playlistId);
    const int rowsBefore = libraryRowCount();
    nmltest::copyAudio(getTestDir(), QStringLiteral("cover-test.ogg"), m_nmlDir);

    const NmlRereadPlan plan = planNmlReread(playlistId, trackCollectionManager());
    const NmlRereadCounts counts = nmlRereadCounts(plan, trackCollectionManager());
    EXPECT_EQ(0, counts.added); // the relinked track is the same library track
    EXPECT_EQ(0, counts.removed);
    const NmlRereadResult result =
            applyNmlReread(plan, NmlRereadMode::Replace, trackCollectionManager());
    ASSERT_TRUE(result.error.isEmpty());
    EXPECT_EQ(imported, tracksInOrder(playlistId));
    EXPECT_EQ(rowsBefore, libraryRowCount());
    EXPECT_FALSE(isFlaggedMissing(imported.at(2)));
    EXPECT_EQ(m_nmlDir.filePath(QStringLiteral("cover-test.ogg")),
            trackCollectionManager()->getTrackById(imported.at(2))->getLocation());
}

// ---- finding the playlist, moved .nml, lock ----

TEST_F(NmlRereadTest, FallsBackToTheNameWhenTheUuidIsGone) {
    const int playlistId = importSet();
    NmlSourceLink link = *loadNmlSourceLink(database(), playlistId);
    link.uuid = QStringLiteral("0000000000000000000000000000dead");
    ASSERT_TRUE(saveNmlSourceLink(database(), link));
    const NmlRereadPlan plan = planNmlReread(playlistId, trackCollectionManager());
    EXPECT_TRUE(plan.error.isEmpty()) << plan.error.toStdString();
    EXPECT_EQ(4, plan.plan.tracks.size());
}

TEST_F(NmlRereadTest, ErrorsWhenNeitherUuidNorNameIsInTheNml) {
    const int playlistId = importSet();
    NmlSourceLink link = *loadNmlSourceLink(database(), playlistId);
    link.uuid = QStringLiteral("0000000000000000000000000000dead");
    link.name = QStringLiteral("Not there");
    ASSERT_TRUE(saveNmlSourceLink(database(), link));
    EXPECT_FALSE(planNmlReread(playlistId, trackCollectionManager()).error.isEmpty());
}

TEST_F(NmlRereadTest, MovedNmlAsksForABrowseAndRemembersTheNewPath) {
    const int playlistId = importSet();
    QTemporaryDir newDir;
    ASSERT_TRUE(newDir.isValid());
    const QString moved = QDir(newDir.path()).filePath(QStringLiteral("My Set.nml"));
    ASSERT_TRUE(QFile::copy(m_nml, moved));
    ASSERT_TRUE(QFile::remove(m_nml));

    const NmlRereadPlan notFound = planNmlReread(playlistId, trackCollectionManager());
    EXPECT_TRUE(notFound.nmlFileMissing);
    EXPECT_FALSE(notFound.error.isEmpty());

    const NmlRereadPlan plan = planNmlReread(playlistId, trackCollectionManager(), moved);
    ASSERT_TRUE(plan.error.isEmpty()) << plan.error.toStdString();
    const NmlRereadResult result =
            applyNmlReread(plan, NmlRereadMode::Keep, trackCollectionManager());
    ASSERT_TRUE(result.error.isEmpty());
    EXPECT_EQ(moved, loadNmlSourceLink(database(), playlistId)->nmlPath);
    // The next re-read needs no browse.
    EXPECT_FALSE(planNmlReread(playlistId, trackCollectionManager()).nmlFileMissing);
}

TEST_F(NmlRereadTest, LockedPlaylistIsRefused) {
    const int playlistId = importSet();
    const QList<TrackId> before = tracksInOrder(playlistId);
    playlistDao().setPlaylistLocked(playlistId, true);
    const NmlRereadPlan plan = planNmlReread(playlistId, trackCollectionManager());
    EXPECT_TRUE(plan.locked);
    EXPECT_FALSE(plan.error.isEmpty());
    // Even a plan made before the lock is refused by Apply.
    playlistDao().setPlaylistLocked(playlistId, false);
    const NmlRereadPlan okPlan = planNmlReread(playlistId, trackCollectionManager());
    playlistDao().setPlaylistLocked(playlistId, true);
    const NmlRereadResult result =
            applyNmlReread(okPlan, NmlRereadMode::Replace, trackCollectionManager());
    EXPECT_FALSE(result.error.isEmpty());
    EXPECT_EQ(before, tracksInOrder(playlistId));
}

} // namespace
