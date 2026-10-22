#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QSqlQuery>
#include <QStandardItem>
#include <memory>

#include "control/controlobject.h"
#include "library/baseexternalplaylistmodel.h"
#include "library/baseexternaltrackmodel.h"
#include "library/basetrackcache.h"
#include "library/browse/browsetablemodel.h"
#include "library/coverlookup.h"
#include "library/keymatch/keymatcher.h"
#include "library/keymatch/keymatchsettings.h"
#include "library/trackcollection.h"
#include "mixer/playerinfo.h"
#include "test/keymatchtestutils.h"
#include "test/librarytest.h"
#include "track/keyutils.h"

// Key matching in Computer (and Quick Links) and the external libraries
// (ticket #102): the same colours as a library list, at the cell seam.
namespace {

using keymatchtest::kGreen;
using keymatchtest::kYellow;
using keymatchtest::KeyMatchTestBed;

// Traktor's tables, which carry the key as text (the lists share the external
// base models).
const QString kLibraryTable = QStringLiteral("traktor_library");
const QString kPlaylistsTable = QStringLiteral("traktor_playlists");
const QString kPlaylistTracksTable = QStringLiteral("traktor_playlist_tracks");

class KeyMatchListsTest : public LibraryTest {
  protected:
    KeyMatchListsTest()
            : m_bed(config(), 1, 0),
              m_coverLookup(internalCollection()) {
    }

    void referenceIs(const QString& camelot) {
        m_bed.referenceIs(camelot);
    }

    // Computer.
    std::unique_ptr<BrowseTableModel> browseModel() {
        auto pModel = std::make_unique<BrowseTableModel>(
                nullptr, trackCollectionManager(), nullptr, &m_coverLookup);
        pModel->setItem(0, COLUMN_FILENAME, new QStandardItem(QStringLiteral("a.mp3")));
        pModel->setItem(0, COLUMN_KEY, new QStandardItem());
        return pModel;
    }

    static QVariant browseKeyColor(BrowseTableModel* pModel, const QString& tagKey) {
        pModel->item(0, COLUMN_KEY)->setText(tagKey);
        return pModel->index(0, COLUMN_KEY).data(Qt::ForegroundRole);
    }

    // External libraries.
    int addExternalTrack(const QString& key) {
        QSqlQuery query(internalCollection()->database());
        query.prepare(QStringLiteral(
                "INSERT INTO traktor_library (artist, title, location, key) "
                "VALUES ('Artist', 'Title', '/nowhere/a.mp3', :key)"));
        query.bindValue(":key", key);
        if (!query.exec()) {
            ADD_FAILURE() << "Adding external track failed";
            return -1;
        }
        return query.lastInsertId().toInt();
    }

    QSharedPointer<BaseTrackCache> trackSource() {
        return QSharedPointer<BaseTrackCache>::create(
                internalCollection(),
                kLibraryTable,
                QStringLiteral("id"),
                QStringList{QStringLiteral("id"),
                        QStringLiteral("artist"),
                        QStringLiteral("title"),
                        QStringLiteral("location"),
                        QStringLiteral("key")},
                QStringList{QStringLiteral("artist"), QStringLiteral("title")},
                false);
    }

    std::unique_ptr<BaseExternalTrackModel> externalModel(
            const QSharedPointer<BaseTrackCache>& pSource) {
        auto pModel = std::make_unique<BaseExternalTrackModel>(nullptr,
                trackCollectionManager(),
                "mixxx.db.model.test_external",
                kLibraryTable,
                pSource);
        pModel->select();
        return pModel;
    }

    static QVariant externalKeyColor(BaseExternalTrackModel* pModel) {
        return pModel->index(0, pModel->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_KEY))
                .data(Qt::ForegroundRole);
    }

    void colourEveryColumn() {
        m_bed.changeSettings([](auto& settings) { settings.wholeRow = true; });
    }

    void changeReferenceTo3B() {
        m_bed.changeReferenceTo3B();
    }

    KeyMatchTestBed m_bed;
    CoverLookup m_coverLookup;
};

} // namespace

TEST_F(KeyMatchListsTest, ComputerColoursTheKeyOfMatchingTracks) {
    referenceIs(QStringLiteral("8A"));
    const auto pModel = browseModel();
    EXPECT_EQ(QVariant::fromValue(kGreen), browseKeyColor(pModel.get(), QStringLiteral("9A")));
    EXPECT_EQ(QVariant::fromValue(kYellow), browseKeyColor(pModel.get(), QStringLiteral("10A")));
    EXPECT_FALSE(browseKeyColor(pModel.get(), QStringLiteral("4B")).isValid());
}

TEST_F(KeyMatchListsTest, ComputerUnderstandsEveryTagNotation) {
    referenceIs(QStringLiteral("8A"));
    const auto pModel = browseModel();
    // All spell A minor, which is 8A (Open Key calls it 1m).
    for (const char* key : {"8A", "08A", "1m", "Am", "A min", "Amin", "a minor"}) {
        EXPECT_EQ(QVariant::fromValue(kGreen),
                browseKeyColor(pModel.get(), QString::fromLatin1(key)))
                << key;
    }
    // A flat minor is 1A, a fuzzy match of 8A.
    EXPECT_EQ(QVariant::fromValue(kYellow),
            browseKeyColor(pModel.get(), QString::fromUtf8("A\xE2\x99\xAD" "m")));
    // F sharp minor is 11A, too far away.
    EXPECT_FALSE(browseKeyColor(pModel.get(), QString::fromUtf8("F\xE2\x99\xAF" "m")).isValid());
}

TEST_F(KeyMatchListsTest, ComputerNeedsAKeyItUnderstands) {
    referenceIs(QStringLiteral("8A"));
    const auto pModel = browseModel();
    EXPECT_FALSE(browseKeyColor(pModel.get(), QString()).isValid());
    EXPECT_FALSE(browseKeyColor(pModel.get(), QStringLiteral("not a key")).isValid());
}

TEST_F(KeyMatchListsTest, ComputerOnlyColoursTheKeyColumn) {
    referenceIs(QStringLiteral("8A"));
    const auto pModel = browseModel();
    ASSERT_EQ(QVariant::fromValue(kGreen), browseKeyColor(pModel.get(), QStringLiteral("8A")));
    EXPECT_FALSE(pModel->index(0, COLUMN_FILENAME).data(Qt::ForegroundRole).isValid());
}

TEST_F(KeyMatchListsTest, ComputerRepaintsWhenTheReferenceChanges) {
    referenceIs(QStringLiteral("8A"));
    const auto pModel = browseModel();
    ASSERT_EQ(QVariant::fromValue(kGreen), browseKeyColor(pModel.get(), QStringLiteral("8A")));
    QSignalSpy repainted(pModel.get(), &QAbstractItemModel::dataChanged);
    changeReferenceTo3B();
    EXPECT_GE(repainted.count(), 1);
    EXPECT_FALSE(pModel->index(0, COLUMN_KEY).data(Qt::ForegroundRole).isValid());
}

TEST_F(KeyMatchListsTest, ExternalLibraryColoursTheKeyOfMatchingTracks) {
    referenceIs(QStringLiteral("8A"));
    addExternalTrack(QStringLiteral("9A"));
    const auto pSource = trackSource();
    const auto pModel = externalModel(pSource);
    ASSERT_EQ(1, pModel->rowCount());
    EXPECT_EQ(QVariant::fromValue(kGreen), externalKeyColor(pModel.get()));
}

TEST_F(KeyMatchListsTest, ExternalLibraryUnderstandsEveryNotation) {
    referenceIs(QStringLiteral("8A"));
    addExternalTrack(QStringLiteral("Am"));
    const auto pSource = trackSource();
    const auto pModel = externalModel(pSource);
    ASSERT_EQ(1, pModel->rowCount());
    EXPECT_EQ(QVariant::fromValue(kGreen), externalKeyColor(pModel.get()));
}

TEST_F(KeyMatchListsTest, ExternalLibraryWithoutAKeyIsNotHighlighted) {
    referenceIs(QStringLiteral("8A"));
    addExternalTrack(QString());
    const auto pSource = trackSource();
    const auto pModel = externalModel(pSource);
    ASSERT_EQ(1, pModel->rowCount());
    EXPECT_FALSE(externalKeyColor(pModel.get()).isValid());
}

TEST_F(KeyMatchListsTest, ExternalPlaylistColoursTheKeyOfMatchingTracks) {
    referenceIs(QStringLiteral("8A"));
    const int trackId = addExternalTrack(QStringLiteral("8B"));
    QSqlQuery query(internalCollection()->database());
    ASSERT_TRUE(query.exec(QStringLiteral("INSERT INTO traktor_playlists (name) VALUES ('Set')")));
    const int playlistId = query.lastInsertId().toInt();
    query.prepare(QStringLiteral(
            "INSERT INTO traktor_playlist_tracks (playlist_id, track_id, position) "
            "VALUES (:playlist, :track, 1)"));
    query.bindValue(":playlist", playlistId);
    query.bindValue(":track", trackId);
    ASSERT_TRUE(query.exec());
    const auto pSource = trackSource();
    BaseExternalPlaylistModel model(nullptr,
            trackCollectionManager(),
            "mixxx.db.model.test_external_playlist",
            kPlaylistsTable,
            kPlaylistTracksTable,
            pSource);
    model.setPlaylistById(playlistId);
    model.select();
    ASSERT_EQ(1, model.rowCount());
    EXPECT_EQ(QVariant::fromValue(kGreen),
            model.index(0, model.fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_KEY))
                    .data(Qt::ForegroundRole));
}

TEST_F(KeyMatchListsTest, ExternalLibraryRepaintsWhenTheReferenceChanges) {
    referenceIs(QStringLiteral("8A"));
    addExternalTrack(QStringLiteral("8A"));
    const auto pSource = trackSource();
    const auto pModel = externalModel(pSource);
    QSignalSpy repainted(pModel.get(), &QAbstractItemModel::dataChanged);
    changeReferenceTo3B();
    EXPECT_GE(repainted.count(), 1);
    EXPECT_FALSE(externalKeyColor(pModel.get()).isValid());
}

TEST_F(KeyMatchListsTest, ComputerColoursTheChosenColumnsAndBoldsThem) {
    referenceIs(QStringLiteral("8A"));
    colourEveryColumn();
    const auto pModel = browseModel();
    pModel->setItem(0, COLUMN_ARTIST, new QStandardItem(QStringLiteral("Artist")));
    ASSERT_EQ(QVariant::fromValue(kGreen), browseKeyColor(pModel.get(), QStringLiteral("8A")));
    EXPECT_EQ(QVariant::fromValue(kGreen), pModel->index(0, COLUMN_ARTIST).data(Qt::ForegroundRole));
    EXPECT_TRUE(pModel->index(0, COLUMN_ARTIST).data(Qt::FontRole).value<QFont>().bold());
    // Never coloured.
    EXPECT_FALSE(pModel->index(0, COLUMN_PREVIEW).data(Qt::ForegroundRole).isValid());
    EXPECT_FALSE(pModel->index(0, COLUMN_COVERART).data(Qt::ForegroundRole).isValid());
}

TEST_F(KeyMatchListsTest, ExternalLibraryColoursTheChosenColumns) {
    referenceIs(QStringLiteral("8A"));
    colourEveryColumn();
    addExternalTrack(QStringLiteral("8A"));
    const auto pSource = trackSource();
    const auto pModel = externalModel(pSource);
    ASSERT_EQ(1, pModel->rowCount());
    EXPECT_EQ(QVariant::fromValue(kGreen),
            pModel->index(0, pModel->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_ARTIST))
                    .data(Qt::ForegroundRole));
}
