#include <gtest/gtest.h>

#include <QDir>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlimport.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"
#include "track/track.h"

using namespace mixxx::nml;

namespace {

class NmlImportTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_dir.isValid());
        m_nmlDir = QDir(m_dir.path());
    }

    PlaylistDAO& playlistDao() const {
        return internalCollection()->getPlaylistDAO();
    }

    // The three tracks of the v20 fixture, next to the .nml.
    QString v20NextToNml(const QString& nmlName = QStringLiteral("My Set.nml")) {
        for (const char* f : {"cover-test-vbr.mp3", "cover-test.flac", "cover-test.ogg"}) {
            nmltest::copyAudio(getTestDir(), QString::fromLatin1(f), m_nmlDir);
        }
        return nmltest::writeFixture(getTestDir(),
                QStringLiteral("traktor-v20-playlist.nml"),
                m_nmlDir,
                nmlName);
    }

    QList<TrackId> tracksInOrder(int playlistId) const {
        QSqlQuery query(internalCollection()->database());
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

    QString locationOf(TrackId id) const {
        return trackCollectionManager()->getTrackById(id)->getLocation();
    }

    QTemporaryDir m_dir;
    QDir m_nmlDir;
};

TEST_F(NmlImportTest, PlansV20TracksFoundNextToNml) {
    const QString nml = v20NextToNml();
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    ASSERT_TRUE(plan.error.isEmpty()) << plan.error.toStdString();
    EXPECT_EQ(20, plan.document.version);
    ASSERT_EQ(4, plan.tracks.size());
    for (const NmlPlanTrack& t : plan.tracks) {
        EXPECT_EQ(NmlTrackStatus::FoundNextToNml, t.status);
    }
    EXPECT_EQ(QStringLiteral("Connected"), plan.tracks.at(0).title);
    EXPECT_EQ(m_nmlDir.filePath(QStringLiteral("cover-test-vbr.mp3")),
            plan.tracks.at(0).resolvedPath);
}

TEST_F(NmlImportTest, PlansFoundAtRecordedPath) {
    // Audio lives in a separate folder that the NML records.
    QTemporaryDir audioDir;
    ASSERT_TRUE(audioDir.isValid());
    for (const char* f : {"cover-test-vbr.mp3", "cover-test.flac", "cover-test.ogg"}) {
        nmltest::copyAudio(getTestDir(), QString::fromLatin1(f), QDir(audioDir.path()));
    }
    const QString nml = nmltest::writeFixture(getTestDir(),
            QStringLiteral("traktor-v20-playlist.nml"),
            m_nmlDir,
            QStringLiteral("set.nml"),
            audioDir.path());
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    ASSERT_TRUE(plan.error.isEmpty()) << plan.error.toStdString();
    ASSERT_EQ(4, plan.tracks.size());
    for (const NmlPlanTrack& t : plan.tracks) {
        EXPECT_EQ(NmlTrackStatus::Found, t.status);
    }
    EXPECT_EQ(QDir(audioDir.path()).filePath(QStringLiteral("cover-test.flac")),
            plan.tracks.at(1).resolvedPath);
}

TEST_F(NmlImportTest, MissingFileIsNotFound) {
    const QString nml = v20NextToNml();
    QFile::remove(m_nmlDir.filePath(QStringLiteral("cover-test.flac")));
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    ASSERT_EQ(4, plan.tracks.size());
    EXPECT_EQ(NmlTrackStatus::NotFound, plan.tracks.at(1).status);
    EXPECT_EQ(QStringLiteral("Bumpy"), plan.tracks.at(1).title);
}

TEST_F(NmlImportTest, AppliesV20InOrderKeepingRepeats) {
    const NmlImportPlan plan = planNmlImport(v20NextToNml(), playlistDao());
    const NmlImportResult result =
            applyNmlImport(plan, plan.defaultName, trackCollectionManager());
    ASSERT_TRUE(result.error.isEmpty()) << result.error.toStdString();
    ASSERT_NE(kInvalidPlaylistId, result.playlistId);
    EXPECT_EQ(QStringLiteral("My Set"), playlistDao().getPlaylistName(result.playlistId));
    const QList<TrackId> ids = tracksInOrder(result.playlistId);
    ASSERT_EQ(4, ids.size());
    EXPECT_EQ(ids.at(0), ids.at(3));
    EXPECT_NE(ids.at(0), ids.at(1));
    EXPECT_TRUE(locationOf(ids.at(0)).endsWith(QStringLiteral("cover-test-vbr.mp3")));
    EXPECT_TRUE(locationOf(ids.at(1)).endsWith(QStringLiteral("cover-test.flac")));
    EXPECT_TRUE(locationOf(ids.at(2)).endsWith(QStringLiteral("cover-test.ogg")));
    EXPECT_EQ(3, result.newLibraryTracks);
    EXPECT_TRUE(result.skipped.isEmpty());
}

TEST_F(NmlImportTest, AppliesV19) {
    for (const char* f : {"cover-test.flac", "empty.mp3", "cover-test.ogg"}) {
        nmltest::copyAudio(getTestDir(), QString::fromLatin1(f), m_nmlDir);
    }
    const QString nml = nmltest::writeFixture(getTestDir(),
            QStringLiteral("traktor-v19-playlist.nml"),
            m_nmlDir,
            QStringLiteral("v19.nml"));
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    ASSERT_TRUE(plan.error.isEmpty());
    EXPECT_EQ(19, plan.document.version);
    const NmlImportResult result =
            applyNmlImport(plan, QStringLiteral("v19"), trackCollectionManager());
    ASSERT_TRUE(result.error.isEmpty()) << result.error.toStdString();
    const QList<TrackId> ids = tracksInOrder(result.playlistId);
    ASSERT_EQ(3, ids.size());
    EXPECT_TRUE(locationOf(ids.at(1)).endsWith(QStringLiteral("empty.mp3")));
}

TEST_F(NmlImportTest, SkipsNotFoundAndListsThem) {
    const QString nml = v20NextToNml();
    QFile::remove(m_nmlDir.filePath(QStringLiteral("cover-test.flac")));
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    const NmlImportResult result =
            applyNmlImport(plan, plan.defaultName, trackCollectionManager());
    EXPECT_EQ(3, tracksInOrder(result.playlistId).size());
    ASSERT_EQ(1, result.skipped.size());
    EXPECT_TRUE(result.skipped.at(0).contains(QStringLiteral("Bumpy")));
}

TEST_F(NmlImportTest, NewTracksTakeTagsFromAudioNotNml) {
    const NmlImportPlan plan = planNmlImport(v20NextToNml(), playlistDao());
    const NmlImportResult result =
            applyNmlImport(plan, plan.defaultName, trackCollectionManager());
    const TrackPointer pTrack =
            trackCollectionManager()->getTrackById(tracksInOrder(result.playlistId).at(0));
    ASSERT_TRUE(pTrack);
    // The NML says artist "Salona"; the audio file's tag wins.
    EXPECT_NE(QStringLiteral("Salona"), pTrack->getArtist());
    EXPECT_NE(QStringLiteral("Connected"), pTrack->getTitle());
}

TEST_F(NmlImportTest, DefaultNameIsFileBaseNameWithNumericSuffixWhenTaken) {
    const QString nml = v20NextToNml();
    EXPECT_EQ(QStringLiteral("My Set"), planNmlImport(nml, playlistDao()).defaultName);

    playlistDao().createPlaylist(QStringLiteral("My Set"));
    EXPECT_EQ(QStringLiteral("My Set 1"), planNmlImport(nml, playlistDao()).defaultName);

    playlistDao().createPlaylist(QStringLiteral("My Set 1"));
    EXPECT_EQ(QStringLiteral("My Set 2"), planNmlImport(nml, playlistDao()).defaultName);
}

TEST_F(NmlImportTest, ApplyUsesEditedNameAndRefusesTakenName) {
    const NmlImportPlan plan = planNmlImport(v20NextToNml(), playlistDao());
    const NmlImportResult ok =
            applyNmlImport(plan, QStringLiteral("Edited"), trackCollectionManager());
    EXPECT_EQ(QStringLiteral("Edited"), playlistDao().getPlaylistName(ok.playlistId));

    const NmlImportResult taken =
            applyNmlImport(plan, QStringLiteral("Edited"), trackCollectionManager());
    EXPECT_EQ(kInvalidPlaylistId, taken.playlistId);
    EXPECT_FALSE(taken.error.isEmpty());
}

TEST_F(NmlImportTest, PlanWritesNothing) {
    const int before = playlistDao().getPlaylistIdFromName(QStringLiteral("My Set"));
    planNmlImport(v20NextToNml(), playlistDao());
    EXPECT_EQ(before, playlistDao().getPlaylistIdFromName(QStringLiteral("My Set")));
    QSqlQuery q(internalCollection()->database());
    ASSERT_TRUE(q.exec(QStringLiteral("SELECT COUNT(*) FROM library")));
    ASSERT_TRUE(q.next());
    EXPECT_EQ(0, q.value(0).toInt());
}

TEST_F(NmlImportTest, UnreadableFileGivesPlanError) {
    const NmlImportPlan plan = planNmlImport(
            m_nmlDir.filePath(QStringLiteral("nope.nml")), playlistDao());
    EXPECT_FALSE(plan.error.isEmpty());
    EXPECT_TRUE(plan.tracks.isEmpty());
}

} // namespace
