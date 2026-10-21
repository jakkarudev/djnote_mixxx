#include "library/trackset/historysessionplaylist.h"

#include <gtest/gtest.h>

#include <QList>
#include <QSqlQuery>

#include "library/dao/playlistdao.h"
#include "test/librarytest.h"
#include "track/track.h"

namespace {

class HistorySessionPlaylistTest : public LibraryTest {
  protected:
    PlaylistDAO& playlistDao() const {
        return internalCollection()->getPlaylistDAO();
    }

    int createSession(const QString& name) const {
        return playlistDao().createPlaylist(name, PlaylistDAO::PLHT_SET_LOG);
    }

    // PlaylistDAO::getTrackIds() is DISTINCT, so read the rows in order.
    QList<TrackId> tracksInOrder(int playlistId) const {
        QSqlQuery query(internalCollection()->database());
        query.prepare(QStringLiteral(
                "SELECT track_id FROM PlaylistTracks WHERE playlist_id = :id "
                "ORDER BY position"));
        query.bindValue(":id", playlistId);
        QList<TrackId> trackIds;
        if (!query.exec()) {
            ADD_FAILURE() << "Reading playlist tracks failed";
            return trackIds;
        }
        while (query.next()) {
            trackIds.append(TrackId(query.value(0)));
        }
        return trackIds;
    }

    TrackId trackIdOf(const QString& relativePath) const {
        return getOrAddTrackByLocation(getTestDir().filePath(relativePath))->getId();
    }
};

TEST_F(HistorySessionPlaylistTest, SuggestsPlaylistSuffix) {
    EXPECT_EQ(QStringLiteral("Night (Playlist)"),
            suggestedPlaylistNameForSession(playlistDao(), QStringLiteral("Night")));
}

TEST_F(HistorySessionPlaylistTest, SuggestsNextFreeNumberedName) {
    playlistDao().createPlaylist(QStringLiteral("Night (Playlist)"));
    EXPECT_EQ(QStringLiteral("Night (Playlist 2)"),
            suggestedPlaylistNameForSession(playlistDao(), QStringLiteral("Night")));

    playlistDao().createPlaylist(QStringLiteral("Night (Playlist 2)"));
    EXPECT_EQ(QStringLiteral("Night (Playlist 3)"),
            suggestedPlaylistNameForSession(playlistDao(), QStringLiteral("Night")));
}

TEST_F(HistorySessionPlaylistTest, SavesTracksInOrderKeepingRepeats) {
    const TrackId a = trackIdOf(QStringLiteral("id3-test-data/cover-test-png.mp3"));
    const TrackId b = trackIdOf(QStringLiteral("id3-test-data/empty.mp3"));
    ASSERT_TRUE(a.isValid() && b.isValid());
    const int sessionId = createSession(QStringLiteral("Night"));
    playlistDao().appendTracksToPlaylist(QList<TrackId>{a, b, a}, sessionId);

    const int newId = saveSessionAsPlaylist(playlistDao(), sessionId, QStringLiteral("Keep"));

    ASSERT_NE(kInvalidPlaylistId, newId);
    EXPECT_EQ(PlaylistDAO::PLHT_NOT_HIDDEN, playlistDao().getHiddenType(newId));
    EXPECT_EQ(QStringLiteral("Keep"), playlistDao().getPlaylistName(newId));
    EXPECT_EQ((QList<TrackId>{a, b, a}), tracksInOrder(newId));
}

TEST_F(HistorySessionPlaylistTest, SavedPlaylistIsASnapshot) {
    const TrackId a = trackIdOf(QStringLiteral("id3-test-data/cover-test-png.mp3"));
    const TrackId b = trackIdOf(QStringLiteral("id3-test-data/empty.mp3"));
    const int sessionId = createSession(QStringLiteral("Night"));
    playlistDao().appendTrackToPlaylist(a, sessionId);

    const int newId = saveSessionAsPlaylist(playlistDao(), sessionId, QStringLiteral("Keep"));
    playlistDao().appendTrackToPlaylist(b, sessionId);

    EXPECT_EQ((QList<TrackId>{a}), tracksInOrder(newId));
    EXPECT_EQ(2, playlistDao().tracksInPlaylist(sessionId));
}

} // namespace
