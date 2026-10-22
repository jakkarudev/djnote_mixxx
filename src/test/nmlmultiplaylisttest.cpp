#include <gtest/gtest.h>

#include <QDialogButtonBox>
#include <QDir>
#include <QPushButton>
#include <QTableWidget>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "library/dao/playlistdao.h"
#include "library/nml/dlgnmlimportplaylists.h"
#include "library/nml/nmlimport.h"
#include "test/librarytest.h"
#include "test/nmltestutils.h"
#include "track/track.h"

using namespace mixxx::nml;

// DJNote: NML files with several playlists (#113), tested at the Plan/Apply seam.

namespace {

class NmlMultiPlaylistTest : public LibraryTest {
  protected:
    void SetUp() override {
        ASSERT_TRUE(m_dir.isValid());
        m_nmlDir = QDir(m_dir.path());
        for (const char* f : {"cover-test-vbr.mp3", "cover-test.flac", "cover-test.ogg"}) {
            nmltest::copyAudio(getTestDir(), QString::fromLatin1(f), m_nmlDir);
        }
    }

    PlaylistDAO& playlistDao() const {
        return internalCollection()->getPlaylistDAO();
    }

    QString fixture(const QString& name, const QString& destName) {
        return nmltest::writeFixture(getTestDir(), name, m_nmlDir, destName);
    }

    QStringList fileNamesInOrder(int playlistId) const {
        QSqlQuery query(internalCollection()->database());
        query.prepare(QStringLiteral(
                "SELECT track_id FROM PlaylistTracks WHERE playlist_id = :id "
                "ORDER BY position"));
        query.bindValue(":id", playlistId);
        EXPECT_TRUE(query.exec());
        QStringList names;
        while (query.next()) {
            names.append(QFileInfo(trackCollectionManager()
                                           ->getTrackById(TrackId(query.value(0)))
                                           ->getLocation())
                                 .fileName());
        }
        return names;
    }

    QTemporaryDir m_dir;
    QDir m_nmlDir;
};

TEST_F(NmlMultiPlaylistTest, OffersOnlyRealPlaylistsNamedAsInTheFile) {
    const QString nml = fixture(QStringLiteral("traktor-multi-playlist.nml"), "collection.nml");
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    ASSERT_TRUE(plan.error.isEmpty()) << plan.error.toStdString();
    // Two playlists inside a folder, one smartlist, _LOOPS, _RECORDINGS and
    // Preparation: exactly the two real playlists remain.
    const QList<NmlPlaylistChoice> choices = nmlPlaylistChoices(plan, playlistDao());
    ASSERT_EQ(2, choices.size());
    EXPECT_EQ(QStringLiteral("Warmup"), choices.at(0).name);
    EXPECT_EQ(2, choices.at(0).trackCount);
    EXPECT_EQ(QStringLiteral("Peak"), choices.at(1).name);
    EXPECT_EQ(3, choices.at(1).trackCount);
    EXPECT_TRUE(choices.at(0).selected);
    EXPECT_TRUE(choices.at(1).selected);
}

TEST_F(NmlMultiPlaylistTest, FirstPlanIsTheFirstImportablePlaylist) {
    const QString nml = fixture(QStringLiteral("traktor-multi-playlist.nml"), "collection.nml");
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    ASSERT_TRUE(plan.error.isEmpty());
    EXPECT_EQ(2, plan.tracks.size()); // Warmup, not _LOOPS
}

TEST_F(NmlMultiPlaylistTest, BothTickedMakeTwoFlatPlaylistsInOrder) {
    const QString nml = fixture(QStringLiteral("traktor-multi-playlist.nml"), "collection.nml");
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    const NmlMultiImportResult multi = applyNmlImportPlaylists(
            plan, nmlPlaylistChoices(plan, playlistDao()), trackCollectionManager());
    ASSERT_TRUE(multi.error.isEmpty());
    ASSERT_EQ(2, multi.results.size());
    ASSERT_TRUE(multi.results.at(0).error.isEmpty());
    ASSERT_TRUE(multi.results.at(1).error.isEmpty());
    EXPECT_EQ(playlistDao().getPlaylistIdFromName("Warmup"), multi.results.at(0).playlistId);
    EXPECT_EQ(playlistDao().getPlaylistIdFromName("Peak"), multi.results.at(1).playlistId);
    EXPECT_EQ((QStringList{"cover-test-vbr.mp3", "cover-test.flac"}),
            fileNamesInOrder(multi.results.at(0).playlistId));
    EXPECT_EQ((QStringList{"cover-test.ogg", "cover-test.flac", "cover-test-vbr.mp3"}),
            fileNamesInOrder(multi.results.at(1).playlistId));
    // Nothing else was created (no _LOOPS, Sets folder, Preparation, smartlist).
    EXPECT_EQ(kInvalidPlaylistId, playlistDao().getPlaylistIdFromName("_LOOPS"));
    EXPECT_EQ(kInvalidPlaylistId, playlistDao().getPlaylistIdFromName("Sets"));
    EXPECT_EQ(kInvalidPlaylistId, playlistDao().getPlaylistIdFromName("Preparation"));
}

TEST_F(NmlMultiPlaylistTest, UntickedPlaylistIsNotImported) {
    const QString nml = fixture(QStringLiteral("traktor-multi-playlist.nml"), "collection.nml");
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    QList<NmlPlaylistChoice> choices = nmlPlaylistChoices(plan, playlistDao());
    choices[0].selected = false;
    const NmlMultiImportResult multi =
            applyNmlImportPlaylists(plan, choices, trackCollectionManager());
    ASSERT_EQ(1, multi.results.size());
    EXPECT_EQ(kInvalidPlaylistId, playlistDao().getPlaylistIdFromName("Warmup"));
    EXPECT_NE(kInvalidPlaylistId, playlistDao().getPlaylistIdFromName("Peak"));
}

TEST_F(NmlMultiPlaylistTest, EditedNameIsUsed) {
    const QString nml = fixture(QStringLiteral("traktor-multi-playlist.nml"), "collection.nml");
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    QList<NmlPlaylistChoice> choices = nmlPlaylistChoices(plan, playlistDao());
    choices[1].name = QStringLiteral("Closing");
    const NmlMultiImportResult multi =
            applyNmlImportPlaylists(plan, choices, trackCollectionManager());
    ASSERT_EQ(2, multi.results.size());
    EXPECT_NE(kInvalidPlaylistId, playlistDao().getPlaylistIdFromName("Closing"));
    EXPECT_EQ(kInvalidPlaylistId, playlistDao().getPlaylistIdFromName("Peak"));
}

TEST_F(NmlMultiPlaylistTest, TakenNamesGetNumericSuffix) {
    ASSERT_NE(kInvalidPlaylistId, playlistDao().createPlaylist("Warmup"));
    const QString nml = fixture(QStringLiteral("traktor-multi-playlist.nml"), "collection.nml");
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    QList<NmlPlaylistChoice> choices = nmlPlaylistChoices(plan, playlistDao());
    EXPECT_EQ(QStringLiteral("Warmup 1"), choices.at(0).name);
    // Two rows renamed to the same name: the second gets the suffix.
    choices[0].name = QStringLiteral("Same");
    choices[1].name = QStringLiteral("Same");
    const NmlMultiImportResult multi =
            applyNmlImportPlaylists(plan, choices, trackCollectionManager());
    ASSERT_EQ(2, multi.results.size());
    EXPECT_EQ(QStringLiteral("Same"), multi.playlistNames.at(0));
    EXPECT_EQ(QStringLiteral("Same 1"), multi.playlistNames.at(1));
    EXPECT_TRUE(multi.results.at(1).error.isEmpty());
    EXPECT_NE(multi.results.at(0).playlistId, multi.results.at(1).playlistId);
}

TEST_F(NmlMultiPlaylistTest, SinglePlaylistFileKeepsFileNameDefault) {
    const QString nml = fixture(QStringLiteral("traktor-v20-playlist.nml"), "My Set.nml");
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    const QList<NmlPlaylistChoice> choices = nmlPlaylistChoices(plan, playlistDao());
    ASSERT_EQ(1, choices.size()); // a lone "Preparation" is the DJ's own list
    EXPECT_EQ(QStringLiteral("My Set"), choices.at(0).name);
}

TEST_F(NmlMultiPlaylistTest, HistoryFileImportsAsOnePlaylist) {
    const QString nml = fixture(QStringLiteral("traktor-history.nml"), "history_2026.nml");
    const NmlImportPlan plan = planNmlImport(nml, playlistDao());
    ASSERT_TRUE(plan.error.isEmpty());
    const QList<NmlPlaylistChoice> choices = nmlPlaylistChoices(plan, playlistDao());
    ASSERT_EQ(1, choices.size());
    const NmlMultiImportResult multi =
            applyNmlImportPlaylists(plan, choices, trackCollectionManager());
    ASSERT_EQ(1, multi.results.size());
    EXPECT_EQ(3, multi.results.at(0).tracksInPlaylist);
    EXPECT_EQ(QStringLiteral("history_2026"), multi.playlistNames.at(0));
}

TEST_F(NmlMultiPlaylistTest, ReviewDialogReportsTicksAndRequiresOne) {
    QList<NmlPlaylistChoice> choices(2);
    choices[0].name = "A";
    choices[1].name = "B";
    DlgNmlImportPlaylists dlg(choices);
    EXPECT_EQ(2, dlg.choices().size());
    EXPECT_TRUE(dlg.choices().at(0).selected);
    auto* pTable = dlg.findChild<QTableWidget*>();
    ASSERT_NE(nullptr, pTable);
    pTable->item(0, 0)->setCheckState(Qt::Unchecked);
    pTable->item(1, 0)->setText("Renamed");
    EXPECT_FALSE(dlg.choices().at(0).selected);
    EXPECT_EQ(QStringLiteral("Renamed"), dlg.choices().at(1).name);
    auto* pOk = dlg.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    EXPECT_TRUE(pOk->isEnabled());
    pTable->item(1, 0)->setCheckState(Qt::Unchecked);
    EXPECT_FALSE(pOk->isEnabled());
}

TEST_F(NmlMultiPlaylistTest, OnlySpecialListsMeansNothingToImport) {
    EXPECT_TRUE(importableNmlPlaylists(NmlDocument()).isEmpty());
}

} // namespace
