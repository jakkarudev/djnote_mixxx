#include "library/externalcovercolumn.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "library/baseexternalplaylistmodel.h"
#include "library/baseexternaltrackmodel.h"
#include "library/basetrackcache.h"
#include "library/coverlookup.h"
#include "library/dao/settingsdao.h"
#include "proto/headers.pb.h"
#include "test/librarytest.h"
#include "track/track.h"

// Readable gtest failure messages.
void PrintTo(const CoverInfo& coverInfo, std::ostream* pOut);

namespace {

const QString kEmbeddedPngTrack = QStringLiteral("id3-test-data/cover-test-png.mp3");
const QString kReferenceJpg = QStringLiteral("id3-test-data/cover_test.jpg");
const QString kReferencePng = QStringLiteral("id3-test-data/reference_cover.png");

// The external library tables of Rhythmbox, which uses the shared external
// base models as they are.
const QString kLibraryTable = QStringLiteral("rhythmbox_library");
const QString kPlaylistsTable = QStringLiteral("rhythmbox_playlists");
const QString kPlaylistTracksTable = QStringLiteral("rhythmbox_playlist_tracks");

class ExternalCoverColumnTest : public LibraryTest {
  protected:
    ExternalCoverColumnTest()
            : m_trackSource(QSharedPointer<BaseTrackCache>::create(
                      internalCollection(),
                      kLibraryTable,
                      QStringLiteral("id"),
                      QStringList{QStringLiteral("id"),
                              QStringLiteral("artist"),
                              QStringLiteral("title"),
                              QStringLiteral("location")},
                      QStringList{QStringLiteral("artist"),
                              QStringLiteral("title")},
                      false)),
              m_coverLookup(internalCollection()) {
    }

    QString testFile(const QString& relativePath) const {
        return getTestDir().filePath(relativePath);
    }

    /// Adds a track to the external library (not to Mixxx's library).
    int addExternalTrack(const QString& location) {
        QSqlQuery query(internalCollection()->database());
        query.prepare(QStringLiteral(
                "INSERT INTO rhythmbox_library (artist, title, location) "
                "VALUES ('Artist', :title, :location)"));
        query.bindValue(":title", QFileInfo(location).fileName());
        query.bindValue(":location", location);
        if (!query.exec()) {
            ADD_FAILURE() << "Adding external track failed";
            return -1;
        }
        return query.lastInsertId().toInt();
    }

    int addExternalPlaylist(const QList<int>& trackIds) {
        QSqlQuery query(internalCollection()->database());
        if (!query.exec(QStringLiteral(
                    "INSERT INTO rhythmbox_playlists (name) VALUES ('Set')"))) {
            ADD_FAILURE() << "Adding external playlist failed";
            return -1;
        }
        const int playlistId = query.lastInsertId().toInt();
        int position = 0;
        for (int trackId : trackIds) {
            query.prepare(QStringLiteral(
                    "INSERT INTO rhythmbox_playlist_tracks "
                    "(playlist_id, track_id, position) "
                    "VALUES (:playlist, :track, :position)"));
            query.bindValue(":playlist", playlistId);
            query.bindValue(":track", trackId);
            query.bindValue(":position", ++position);
            if (!query.exec()) {
                ADD_FAILURE() << "Adding external playlist track failed";
            }
        }
        return playlistId;
    }

    std::unique_ptr<BaseExternalTrackModel> trackListModel() {
        auto pModel = std::make_unique<BaseExternalTrackModel>(nullptr,
                trackCollectionManager(),
                "mixxx.db.model.test_external",
                kLibraryTable,
                m_trackSource);
        // As Library does when it shows the model.
        ExternalCoverColumn::attachCoverLookup(pModel.get(), &m_coverLookup);
        pModel->select();
        return pModel;
    }

    std::unique_ptr<BaseExternalPlaylistModel> playlistModel(int playlistId) {
        auto pModel = std::make_unique<BaseExternalPlaylistModel>(nullptr,
                trackCollectionManager(),
                "mixxx.db.model.test_external_playlist",
                kPlaylistsTable,
                kPlaylistTracksTable,
                m_trackSource);
        pModel->setPlaylistById(playlistId);
        ExternalCoverColumn::attachCoverLookup(pModel.get(), &m_coverLookup);
        pModel->select();
        return pModel;
    }

    static QModelIndex coverIndex(const BaseSqlTableModel& model, const QString& location) {
        const int column = model.fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART);
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row, column);
            if (model.getTrackLocation(index) == location) {
                return index;
            }
        }
        ADD_FAILURE() << "No row for " << location.toStdString();
        return QModelIndex();
    }

    /// Draws the cover cell of `location` and waits until it is repainted
    /// with the cover read in the background.
    static CoverInfo coverAfterRepaint(BaseSqlTableModel* pModel, const QString& location) {
        const QModelIndex index = coverIndex(*pModel, location);
        QSignalSpy repainted(pModel, &QAbstractItemModel::dataChanged);
        if (pModel->getCoverInfo(index).hasImage()) {
            return pModel->getCoverInfo(index);
        }
        const auto cellRepainted = [&repainted, &index] {
            for (const auto& arguments : std::as_const(repainted)) {
                if (arguments.at(0).value<QModelIndex>() == index &&
                        arguments.at(1).value<QModelIndex>() == index) {
                    return true;
                }
            }
            return false;
        };
        while (!cellRepainted()) {
            if (!repainted.wait(kReadTimeoutMillis)) {
                ADD_FAILURE() << "Cover cell not repainted for " << location.toStdString();
                break;
            }
        }
        return pModel->getCoverInfo(index);
    }

    int libraryTrackCount() const {
        QSqlQuery query(internalCollection()->database());
        if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM library")) || !query.next()) {
            ADD_FAILURE() << "Counting library tracks failed";
            return -1;
        }
        return query.value(0).toInt();
    }

    CoverInfo embeddedCover(const QString& location) const {
        CoverInfo cover;
        cover.type = CoverInfo::METADATA;
        cover.source = CoverInfo::GUESSED;
        cover.setImageDigest(QImage(testFile(kReferencePng)));
        cover.trackLocation = location;
        return cover;
    }

    /// A header state saved by WTrackTableViewHeader, with one entry per
    /// (name, hidden) in visual order, logical index = position.
    void saveHeaderState(const char* settingsNamespace,
            const QList<std::pair<QString, bool>>& columns,
            int sortSection) {
        mixxx::library::HeaderViewState state;
        int index = 0;
        for (const auto& [name, hidden] : columns) {
            auto* pHeader = state.add_header_state();
            pHeader->set_column_name(name.toStdString());
            pHeader->set_hidden(hidden);
            pHeader->set_size(50);
            pHeader->set_logical_index(index);
            pHeader->set_visual_index(index);
            ++index;
        }
        state.set_sort_indicator_shown(true);
        state.set_sort_indicator_section(sortSection);
        state.set_sort_order(Qt::AscendingOrder);
        const std::string bytes = state.SerializeAsString();
        SettingsDAO(internalCollection()->database())
                .setValue(QString::fromLatin1(settingsNamespace) +
                                QStringLiteral(".header_state_pb"),
                        QString(QByteArray(bytes.data(), static_cast<int>(bytes.size()))
                                        .toBase64()));
    }

    static mixxx::library::HeaderViewState savedHeaderState(TrackModel* pModel) {
        const QByteArray bytes = QByteArray::fromBase64(
                pModel->getModelSetting(QStringLiteral("header_state_pb")).toLatin1());
        mixxx::library::HeaderViewState state;
        EXPECT_TRUE(state.ParseFromArray(bytes.constData(), static_cast<int>(bytes.size())));
        return state;
    }

    /// The names of the visible columns in visual order, e.g. "preview artist".
    static std::string visibleColumnNames(const mixxx::library::HeaderViewState& state) {
        std::string names;
        for (const auto& header : state.header_state()) {
            if (!header.hidden()) {
                names += (names.empty() ? "" : " ") + header.column_name();
            }
        }
        return names;
    }

    static constexpr int kReadTimeoutMillis = 5000;

    QSharedPointer<BaseTrackCache> m_trackSource;
    CoverLookup m_coverLookup;
};

TEST_F(ExternalCoverColumnTest, trackListShowsEmbeddedCoverOfFileNotInLibrary) {
    const QString location = testFile(kEmbeddedPngTrack);
    addExternalTrack(location);
    const auto pModel = trackListModel();

    EXPECT_EQ(embeddedCover(location), coverAfterRepaint(pModel.get(), location));
}

TEST_F(ExternalCoverColumnTest, playlistShowsCustomCoverOfLibraryTrack) {
    const QString location = testFile(kEmbeddedPngTrack);
    CoverInfo customCover;
    customCover.type = CoverInfo::FILE;
    customCover.source = CoverInfo::USER_SELECTED;
    customCover.coverLocation = testFile(kReferenceJpg);
    customCover.setImageDigest(QImage(customCover.coverLocation));
    customCover.trackLocation = location;
    {
        const TrackPointer pTrack = getOrAddTrackByLocation(location);
        ASSERT_TRUE(pTrack);
        pTrack->setCoverInfo(customCover);
        // Releasing the track saves it to the library.
    }
    const int playlistId = addExternalPlaylist({addExternalTrack(location)});
    const auto pModel = playlistModel(playlistId);

    EXPECT_EQ(customCover, coverAfterRepaint(pModel.get(), location));
}

TEST_F(ExternalCoverColumnTest, coverColumnIsRightAfterPreviewAsInTracks) {
    const int playlistId = addExternalPlaylist({addExternalTrack(testFile(kEmbeddedPngTrack))});
    const auto pTrackList = trackListModel();
    const auto pPlaylist = playlistModel(playlistId);

    EXPECT_EQ(pTrackList->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_PREVIEW) + 1,
            pTrackList->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART));
    EXPECT_EQ(pPlaylist->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_PREVIEW) + 1,
            pPlaylist->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART));
}

TEST_F(ExternalCoverColumnTest, scrollingTrackListAddsNoTracksToLibrary) {
    QTemporaryDir folder;
    ASSERT_TRUE(folder.isValid());
    const QString withFolderImage = folder.filePath(QStringLiteral("track.mp3"));
    ASSERT_TRUE(QFile::copy(testFile(QStringLiteral("id3-test-data/empty.mp3")),
            withFolderImage));
    ASSERT_TRUE(QFile::copy(testFile(kReferenceJpg), folder.filePath("folder.jpg")));
    const QString missing = folder.filePath(QStringLiteral("missing.mp3"));
    const QString embedded = testFile(kEmbeddedPngTrack);
    addExternalTrack(withFolderImage);
    addExternalTrack(missing);
    addExternalTrack(embedded);
    const int tracksBefore = libraryTrackCount();
    const auto pModel = trackListModel();

    EXPECT_EQ(embeddedCover(embedded), coverAfterRepaint(pModel.get(), embedded));
    EXPECT_EQ(CoverInfo::FILE, coverAfterRepaint(pModel.get(), withFolderImage).type);
    // A lost track: empty cover, nothing read.
    EXPECT_FALSE(coverAfterRepaint(pModel.get(), missing).hasImage());
    EXPECT_EQ(tracksBefore, libraryTrackCount());
}

TEST_F(ExternalCoverColumnTest, headerSavedBeforeCoverColumnShowsItAfterPreview) {
    // Saved before the cover column: id(0) preview(1) artist(2) title(3),
    // sorted by title.
    saveHeaderState("mixxx.db.model.test_external",
            {{QStringLiteral("id"), true},
                    {QStringLiteral("preview"), false},
                    {QStringLiteral("artist"), false},
                    {QStringLiteral("title"), false}},
            3);

    const auto pModel = trackListModel();

    const auto state = savedHeaderState(pModel.get());
    EXPECT_EQ("preview coverart artist title", visibleColumnNames(state));
    // Still sorted by title, which moved one to the right.
    EXPECT_EQ(pModel->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_TITLE),
            state.sort_indicator_section());
}

TEST_F(ExternalCoverColumnTest, playlistHeaderSavedBeforeCoverColumnShowsIt) {
    const int playlistId = addExternalPlaylist({addExternalTrack(testFile(kEmbeddedPngTrack))});
    saveHeaderState("mixxx.db.model.test_external_playlist",
            {{QStringLiteral("position"), false},
                    {QStringLiteral("preview"), false},
                    {QStringLiteral("title"), false}},
            0);

    const auto pModel = playlistModel(playlistId);

    EXPECT_EQ("position preview coverart title",
            visibleColumnNames(savedHeaderState(pModel.get())));
}

TEST_F(ExternalCoverColumnTest, coverColumnHiddenByTheDjStaysHidden) {
    saveHeaderState("mixxx.db.model.test_external",
            {{QStringLiteral("preview"), false},
                    {QStringLiteral("coverart"), true},
                    {QStringLiteral("artist"), false}},
            2);

    const auto pModel = trackListModel();

    EXPECT_EQ("preview artist", visibleColumnNames(savedHeaderState(pModel.get())));
}

} // namespace
