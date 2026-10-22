#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QSqlQuery>
#include <memory>

#include "control/controlobject.h"
#include "library/basetrackcache.h"
#include "library/columncache.h"
#include "library/dao/trackschema.h"
#include "library/keymatch/keymatcher.h"
#include "library/keymatch/keymatchcell.h"
#include "library/keymatch/keymatchsettings.h"
#include "library/basetracktablemodel.h"
#include "library/librarytablemodel.h"
#include "library/missing_hidden/missingtablemodel.h"
#include "library/queryutil.h"
#include "mixer/playerinfo.h"
#include "test/librarytest.h"
#include "track/keyutils.h"
#include "track/track.h"
#include "library/trackcollection.h"
#include "widget/wtracktableview.h"

// What a track list cell shows under key matching (ticket #96): a real library
// track list, with the deck controls and the settings driven the way the app
// drives them.
namespace {

using mixxx::keymatch::Highlight;
using mixxx::keymatch::KeyMatcher;

const QColor kGreen(QStringLiteral("#4ade80"));
const QColor kYellow(QStringLiteral("#facc15"));
// Each tier colour mixed 50/50 with the default played colour #555555.
const QColor kPlayedGreen(80, 154, 107);
const QColor kPlayedYellow(168, 145, 53);
const QColor kPurple(QStringLiteral("#c084fc"));
const QColor kLightPurple(QStringLiteral("#e9d5ff"));

class KeyMatchCellTest : public LibraryTest {
  protected:
    KeyMatchCellTest()
            : m_numDecks(ConfigKey(QStringLiteral("[App]"), QStringLiteral("num_decks"))),
              m_play(ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("play"))),
              m_syncLeader(ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("sync_leader"))),
              m_fileKey(ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("file_key"))),
              m_key(ConfigKey(QStringLiteral("[Channel1]"), QStringLiteral("key"))) {
        m_numDecks.set(1);
        PlayerInfo::create();
        m_pMatcher = std::make_unique<KeyMatcher>(config());
        m_pTrack = getOrAddTrackByLocation(
                getTestDir().filePath(QStringLiteral("id3-test-data/artist.mp3")));
        connectTrackSource();
        m_pModel = std::make_unique<LibraryTableModel>(
                nullptr, trackCollectionManager(), "test");
        m_pModel->setTableModel();
    }
    ~KeyMatchCellTest() override {
        BaseTrackTableModel::setApplyPlayedTrackColor(true);
        m_pModel.reset();
        m_pTrack.reset();
        m_pMatcher.reset();
        PlayerInfo::destroy();
    }

    // What the Tracks feature gives the library: the track source the list
    // reads its columns from (a subset of the real columns).
    void connectTrackSource() {
        const QStringList columns = {
                LIBRARYTABLE_ID,
                LIBRARYTABLE_PLAYED,
                LIBRARYTABLE_TIMESPLAYED,
                LIBRARYTABLE_ARTIST,
                LIBRARYTABLE_TITLE,
                LIBRARYTABLE_BPM,
                LIBRARYTABLE_KEY,
                LIBRARYTABLE_KEY_ID,
                TRACKLOCATIONSTABLE_LOCATION,
                TRACKLOCATIONSTABLE_FSDELETED,
                LIBRARYTABLE_MIXXXDELETED,
                LIBRARYTABLE_COLOR,
                LIBRARYTABLE_COVERART_SOURCE,
                LIBRARYTABLE_COVERART_TYPE,
                LIBRARYTABLE_COVERART_LOCATION,
                LIBRARYTABLE_COVERART_COLOR,
                LIBRARYTABLE_COVERART_DIGEST,
                LIBRARYTABLE_COVERART_HASH};
        QStringList qualified;
        for (const auto& col : columns) {
            qualified.append(mixxx::trackschema::tableForColumn(col) + QLatin1Char('.') + col);
        }
        QSqlQuery query(internalCollection()->database());
        ASSERT_TRUE(query.exec(QStringLiteral(
                "CREATE TEMPORARY VIEW IF NOT EXISTS library_cache_view AS "
                "SELECT %1 FROM library "
                "INNER JOIN track_locations ON library.location = track_locations.id")
                                       .arg(qualified.join(","))));
        m_pTrackSource = QSharedPointer<BaseTrackCache>(new BaseTrackCache(internalCollection(),
                QStringLiteral("library_cache_view"),
                LIBRARYTABLE_ID,
                columns,
                {LIBRARYTABLE_ARTIST, LIBRARYTABLE_TITLE},
                true));
        internalCollection()->connectTrackSource(m_pTrackSource);
    }

    // The reference deck: deck 1 plays as sync leader in `camelot`.
    void referenceIs(const QString& camelot) {
        const auto key = KeyUtils::guessKeyFromText(camelot);
        ASSERT_NE(mixxx::track::io::key::INVALID, key);
        m_fileKey.set(KeyUtils::keyToNumericValue(key));
        m_key.set(KeyUtils::keyToNumericValue(key));
        m_syncLeader.set(2);
        m_play.set(1);
    }

    void trackKeyIs(const QString& camelot) {
        m_pTrack->setKeyText(camelot);
        trackCollectionManager()->saveTrack(m_pTrack);
        m_pModel->select();
    }

    QModelIndex cell(ColumnCache::Column column) const {
        const int row = rowOfTrack();
        return m_pModel->index(row, m_pModel->fieldIndex(column));
    }

    int rowOfTrack() const {
        for (int row = 0; row < m_pModel->rowCount(); ++row) {
            if (m_pModel->getTrackId(m_pModel->index(row, 0)) == m_pTrack->getId()) {
                return row;
            }
        }
        ADD_FAILURE() << "track not listed";
        return 0;
    }

    QVariant keyColor() const {
        return cell(ColumnCache::COLUMN_LIBRARYTABLE_KEY).data(Qt::ForegroundRole);
    }

    QVariant keyColorFor(const QString& camelot) {
        trackKeyIs(camelot);
        return keyColor();
    }

    void trackIsPlayed() {
        m_pTrack->updatePlayedStatusKeepPlayCount(true);
        trackCollectionManager()->saveTrack(m_pTrack);
        m_pModel->select();
    }

    void setHighlight(Highlight highlight) {
        mixxx::keymatch::Settings settings = mixxx::keymatch::loadSettings(config());
        settings.highlight = highlight;
        storeSettings(settings);
    }

    void storeSettings(const mixxx::keymatch::Settings& settings) {
        mixxx::keymatch::saveSettings(config(), settings);
        m_pMatcher->reloadSettings();
    }

    void setBoldMatchedText(bool bold) {
        mixxx::keymatch::Settings settings = mixxx::keymatch::loadSettings(config());
        settings.boldMatchedText = bold;
        mixxx::keymatch::saveSettings(config(), settings);
        m_pMatcher->reloadSettings();
    }

    void setMatchShiftedKey(bool match) {
        mixxx::keymatch::Settings settings = mixxx::keymatch::loadSettings(config());
        settings.matchShiftedKey = match;
        mixxx::keymatch::saveSettings(config(), settings);
        m_pMatcher->reloadSettings();
    }

    QVariant fontOf(ColumnCache::Column column) const {
        return cell(column).data(Qt::FontRole);
    }

    bool keyIsBold() const {
        const QVariant font = fontOf(ColumnCache::COLUMN_LIBRARYTABLE_KEY);
        return font.isValid() && font.value<QFont>().bold();
    }

    // The reference deck plays in `stored` at the shifted key `shifted`.
    void referenceIsShifted(const QString& stored, const QString& shifted) {
        referenceIs(stored);
        m_key.set(KeyUtils::keyToNumericValue(KeyUtils::guessKeyFromText(shifted)));
    }

    ControlObject m_numDecks;
    ControlObject m_play;
    ControlObject m_syncLeader;
    ControlObject m_fileKey;
    ControlObject m_key;
    std::unique_ptr<KeyMatcher> m_pMatcher;
    QSharedPointer<BaseTrackCache> m_pTrackSource;
    TrackPointer m_pTrack;
    std::unique_ptr<LibraryTableModel> m_pModel;
};

} // namespace

TEST_F(KeyMatchCellTest, StrictMatchesAreGreenAgainstAMinorReference) {
    referenceIs(QStringLiteral("8A"));
    for (const char* key : {"7A", "8A", "8B", "9A"}) {
        EXPECT_EQ(QVariant::fromValue(kGreen), keyColorFor(QString::fromLatin1(key))) << key;
    }
}

TEST_F(KeyMatchCellTest, FuzzyMatchesAreYellow) {
    referenceIs(QStringLiteral("8A"));
    for (const char* key : {"1A", "3A", "6A", "10A"}) {
        EXPECT_EQ(QVariant::fromValue(kYellow), keyColorFor(QString::fromLatin1(key))) << key;
    }
}

TEST_F(KeyMatchCellTest, OtherKeysAndDiagonalsKeepTheNormalColour) {
    referenceIs(QStringLiteral("8A"));
    for (const char* key : {"7B", "9B", "2A", "4A", "5A", "11A", "12A", "1B", "5B"}) {
        EXPECT_FALSE(keyColorFor(QString::fromLatin1(key)).isValid()) << key;
    }
}

TEST_F(KeyMatchCellTest, MajorReferenceAndTheWrapAroundTheWheel) {
    referenceIs(QStringLiteral("12B"));
    EXPECT_EQ(QVariant::fromValue(kGreen), keyColorFor(QStringLiteral("1B")));
    EXPECT_EQ(QVariant::fromValue(kGreen), keyColorFor(QStringLiteral("11B")));
    EXPECT_EQ(QVariant::fromValue(kGreen), keyColorFor(QStringLiteral("12A")));
    EXPECT_EQ(QVariant::fromValue(kYellow), keyColorFor(QStringLiteral("2B")));
    EXPECT_EQ(QVariant::fromValue(kYellow), keyColorFor(QStringLiteral("10B")));
    EXPECT_FALSE(keyColorFor(QStringLiteral("5A")).isValid());
}

TEST_F(KeyMatchCellTest, TracksWithoutAKeyKeepTheNormalColour) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QString());
    EXPECT_FALSE(keyColor().isValid());
}

TEST_F(KeyMatchCellTest, NothingIsHighlightedWithoutAReferenceDeck) {
    EXPECT_FALSE(keyColorFor(QStringLiteral("8A")).isValid());
}

TEST_F(KeyMatchCellTest, ReferenceWithoutAKeyHighlightsNothing) {
    m_syncLeader.set(2);
    m_play.set(1);
    EXPECT_FALSE(keyColorFor(QStringLiteral("8A")).isValid());
}

TEST_F(KeyMatchCellTest, OnlyTheKeyColumnIsColoured) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    for (ColumnCache::Column column : {ColumnCache::COLUMN_LIBRARYTABLE_ARTIST,
                 ColumnCache::COLUMN_LIBRARYTABLE_TITLE,
                 ColumnCache::COLUMN_LIBRARYTABLE_BPM,
                 ColumnCache::COLUMN_LIBRARYTABLE_COVERART}) {
        EXPECT_FALSE(cell(column).data(Qt::ForegroundRole).isValid()) << column;
    }
}

TEST_F(KeyMatchCellTest, AMissingTrackKeepsTheMissingColour) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    ASSERT_EQ(QVariant::fromValue(kGreen), keyColor());

    QSqlQuery query(dbConnection());
    ASSERT_TRUE(query.exec(QStringLiteral("UPDATE track_locations SET fs_deleted=1")));
    m_pTrackSource->buildIndex();
    // Tracks hides missing tracks; the Missing list shows them.
    MissingTableModel missingModel(nullptr, trackCollectionManager());
    missingModel.select();
    ASSERT_EQ(1, missingModel.rowCount());
    const QModelIndex missingKey = missingModel.index(
            0, missingModel.fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_KEY));
    EXPECT_EQ(QVariant::fromValue(QColor(WTrackTableView::kDefaultTrackMissingColor)),
            missingKey.data(Qt::ForegroundRole));
}

TEST_F(KeyMatchCellTest, HighlightOffShowsNothing) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    setHighlight(Highlight::Off);
    EXPECT_FALSE(keyColor().isValid());
}

TEST_F(KeyMatchCellTest, StrictOnlyHidesYellow) {
    referenceIs(QStringLiteral("8A"));
    setHighlight(Highlight::StrictOnly);
    EXPECT_EQ(QVariant::fromValue(kGreen), keyColorFor(QStringLiteral("9A")));
    EXPECT_FALSE(keyColorFor(QStringLiteral("10A")).isValid());
}

TEST_F(KeyMatchCellTest, ChangingTheReferenceRepaintsWithoutReloading) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    ASSERT_EQ(QVariant::fromValue(kGreen), keyColor());

    QSignalSpy repainted(m_pModel.get(), &QAbstractItemModel::dataChanged);
    QSignalSpy reloaded(m_pModel.get(), &QAbstractItemModel::modelReset);
    const auto key = KeyUtils::guessKeyFromText(QStringLiteral("3B"));
    m_fileKey.set(KeyUtils::keyToNumericValue(key));
    m_key.set(KeyUtils::keyToNumericValue(key));
    EXPECT_GE(repainted.count(), 1);
    EXPECT_EQ(0, reloaded.count());
    EXPECT_FALSE(keyColor().isValid());
}

TEST_F(KeyMatchCellTest, ChangingTheSettingRepaints) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    QSignalSpy repainted(m_pModel.get(), &QAbstractItemModel::dataChanged);
    setHighlight(Highlight::Off);
    EXPECT_GE(repainted.count(), 1);
}

TEST_F(KeyMatchCellTest, APlayedMatchIsMixedWithThePlayedColour) {
    BaseTrackTableModel::setApplyPlayedTrackColor(true);
    referenceIs(QStringLiteral("8A"));
    trackIsPlayed();
    EXPECT_EQ(QVariant::fromValue(kPlayedGreen), keyColorFor(QStringLiteral("8A")));
    EXPECT_EQ(QVariant::fromValue(kPlayedYellow), keyColorFor(QStringLiteral("10A")));
}

TEST_F(KeyMatchCellTest, APlayedMatchIsNotDimmedWithoutPlayedColouring) {
    BaseTrackTableModel::setApplyPlayedTrackColor(false);
    referenceIs(QStringLiteral("8A"));
    trackIsPlayed();
    EXPECT_EQ(QVariant::fromValue(kGreen), keyColorFor(QStringLiteral("8A")));
}

TEST_F(KeyMatchCellTest, PlayedColumnsThatAreNotColouredKeepThePlayedGrey) {
    BaseTrackTableModel::setApplyPlayedTrackColor(true);
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    trackIsPlayed();
    EXPECT_EQ(QVariant::fromValue(QColor(WTrackTableView::kDefaultTrackPlayedColor)),
            cell(ColumnCache::COLUMN_LIBRARYTABLE_ARTIST).data(Qt::ForegroundRole));
}

TEST_F(KeyMatchCellTest, AMissingPlayedTrackKeepsTheMissingColour) {
    BaseTrackTableModel::setApplyPlayedTrackColor(true);
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    trackIsPlayed();
    QSqlQuery query(dbConnection());
    ASSERT_TRUE(query.exec(QStringLiteral("UPDATE track_locations SET fs_deleted=1")));
    m_pTrackSource->buildIndex();
    MissingTableModel missingModel(nullptr, trackCollectionManager());
    missingModel.select();
    ASSERT_EQ(1, missingModel.rowCount());
    const QModelIndex missingKey = missingModel.index(
            0, missingModel.fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_KEY));
    EXPECT_EQ(QVariant::fromValue(QColor(WTrackTableView::kDefaultTrackMissingColor)),
            missingKey.data(Qt::ForegroundRole));
}

// Which columns are coloured (ticket #99).
using mixxx::keymatch::ColumnIdentity;

TEST_F(KeyMatchCellTest, TickedColumnsAreColouredAndUntickedKeepMixxxColours) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    mixxx::keymatch::Settings settings;
    settings.columns = {ColumnIdentity::Title, ColumnIdentity::Bpm};
    storeSettings(settings);
    EXPECT_EQ(QVariant::fromValue(kGreen),
            cell(ColumnCache::COLUMN_LIBRARYTABLE_TITLE).data(Qt::ForegroundRole));
    EXPECT_EQ(QVariant::fromValue(kGreen),
            cell(ColumnCache::COLUMN_LIBRARYTABLE_BPM).data(Qt::ForegroundRole));
    EXPECT_FALSE(cell(ColumnCache::COLUMN_LIBRARYTABLE_ARTIST).data(Qt::ForegroundRole).isValid());
    EXPECT_FALSE(keyColor().isValid());
}

TEST_F(KeyMatchCellTest, WholeRowColoursEveryTextColumnButNoOthers) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("9A"));
    mixxx::keymatch::Settings settings;
    settings.wholeRow = true;
    storeSettings(settings);
    for (ColumnCache::Column column : {ColumnCache::COLUMN_LIBRARYTABLE_KEY,
                 ColumnCache::COLUMN_LIBRARYTABLE_ARTIST,
                 ColumnCache::COLUMN_LIBRARYTABLE_TITLE,
                 ColumnCache::COLUMN_LIBRARYTABLE_BPM,
                 ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION}) {
        EXPECT_EQ(QVariant::fromValue(kGreen), cell(column).data(Qt::ForegroundRole)) << column;
    }
    for (ColumnCache::Column column : {ColumnCache::COLUMN_LIBRARYTABLE_COVERART,
                 ColumnCache::COLUMN_LIBRARYTABLE_COLOR,
                 ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED}) {
        // TIMESPLAYED is the Played checkbox column.
        EXPECT_FALSE(cell(column).data(Qt::ForegroundRole).isValid()) << column;
    }
}

TEST_F(KeyMatchCellTest, ThePlayedCheckboxColumnHasNoIdentity) {
    EXPECT_EQ(ColumnIdentity::NotColoured,
            mixxx::keymatch::columnIdentity(ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED));
}

TEST_F(KeyMatchCellTest, AStoredPlayedTimesColumnLoadsCleanlyAndIsIgnored) {
    config()->setValue(ConfigKey(QStringLiteral("[DJNote]"), QStringLiteral("KeyMatchingColumns")),
            QStringLiteral("timesPlayed,key"));
    const auto settings = mixxx::keymatch::loadSettings(config());
    EXPECT_EQ((std::set<ColumnIdentity>{ColumnIdentity::Key}), settings.columns);
}

TEST_F(KeyMatchCellTest, AColumnTheListLacksIsSkipped) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    mixxx::keymatch::Settings settings;
    settings.columns = {ColumnIdentity::Genre, ColumnIdentity::Key};
    storeSettings(settings);
    EXPECT_EQ(QVariant::fromValue(kGreen), keyColor());
}

TEST_F(KeyMatchCellTest, ChangingTheColumnsRepaints) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    QSignalSpy repainted(m_pModel.get(), &QAbstractItemModel::dataChanged);
    mixxx::keymatch::Settings settings;
    settings.wholeRow = true;
    storeSettings(settings);
    EXPECT_GE(repainted.count(), 1);
}

// Bold matched text (ticket #100).
TEST_F(KeyMatchCellTest, MatchedKeysAreBoldInEveryTier) {
    referenceIs(QStringLiteral("8A"));
    for (const char* key : {"8A", "9A", "10A"}) {
        trackKeyIs(QString::fromLatin1(key));
        EXPECT_TRUE(keyIsBold()) << key;
    }
}

TEST_F(KeyMatchCellTest, UnmatchedKeysAndOtherCellsKeepTheirFont) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("2A"));
    EXPECT_FALSE(fontOf(ColumnCache::COLUMN_LIBRARYTABLE_KEY).isValid());
    trackKeyIs(QStringLiteral("8A"));
    for (ColumnCache::Column column : {ColumnCache::COLUMN_LIBRARYTABLE_ARTIST,
                 ColumnCache::COLUMN_LIBRARYTABLE_TITLE,
                 ColumnCache::COLUMN_LIBRARYTABLE_BPM}) {
        EXPECT_FALSE(fontOf(column).isValid()) << column;
    }
}

TEST_F(KeyMatchCellTest, PlayedMatchesStayBold) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    QSqlQuery query(dbConnection());
    ASSERT_TRUE(query.exec(QStringLiteral("UPDATE library SET played=1")));
    m_pTrackSource->buildIndex();
    m_pModel->select();
    EXPECT_TRUE(keyIsBold());
}

TEST_F(KeyMatchCellTest, BoldOffChangesNoFont) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    setBoldMatchedText(false);
    EXPECT_FALSE(fontOf(ColumnCache::COLUMN_LIBRARYTABLE_KEY).isValid());
    // The colour stays.
    EXPECT_EQ(QVariant::fromValue(kGreen), keyColor());
}

TEST_F(KeyMatchCellTest, HighlightOffIsNotBoldEvenWithBoldOn) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    setHighlight(Highlight::Off);
    EXPECT_FALSE(keyIsBold());
}

TEST_F(KeyMatchCellTest, ChangingBoldRepaints) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    QSignalSpy repainted(m_pModel.get(), &QAbstractItemModel::dataChanged);
    setBoldMatchedText(false);
    EXPECT_GE(repainted.count(), 1);
}

TEST_F(KeyMatchCellTest, AMissingTrackIsNeverBold) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("8A"));
    ASSERT_TRUE(keyIsBold());
    QSqlQuery query(dbConnection());
    ASSERT_TRUE(query.exec(QStringLiteral("UPDATE track_locations SET fs_deleted=1")));
    m_pTrackSource->buildIndex();
    MissingTableModel missingModel(nullptr, trackCollectionManager());
    missingModel.select();
    ASSERT_EQ(1, missingModel.rowCount());
    const QModelIndex missingKey = missingModel.index(
            0, missingModel.fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_KEY));
    EXPECT_FALSE(missingKey.data(Qt::FontRole).isValid());
}

TEST_F(KeyMatchCellTest, ShiftedKeyMatchesArePurple) {
    referenceIsShifted(QStringLiteral("8A"), QStringLiteral("3A"));
    for (const char* key : {"2A", "3B", "4A"}) {
        EXPECT_EQ(QVariant::fromValue(kPurple), keyColorFor(QString::fromLatin1(key))) << key;
    }
    // 8A, 10A and 1A match the stored key (green, yellow) and keep that colour.
    for (const char* key : {"5A"}) {
        EXPECT_EQ(QVariant::fromValue(kLightPurple), keyColorFor(QString::fromLatin1(key))) << key;
    }
}

TEST_F(KeyMatchCellTest, StoredKeyMatchesKeepTheirColourAgainstTheShiftedKey) {
    referenceIsShifted(QStringLiteral("8A"), QStringLiteral("3A"));
    EXPECT_EQ(QVariant::fromValue(kGreen), keyColorFor(QStringLiteral("8A")));
    EXPECT_EQ(QVariant::fromValue(kGreen), keyColorFor(QStringLiteral("9A")));
    // 3A is fuzzy to 8A and strict to 3A: yellow stays.
    EXPECT_EQ(QVariant::fromValue(kYellow), keyColorFor(QStringLiteral("3A")));
}

TEST_F(KeyMatchCellTest, NoPurpleWhenTheShiftedKeyEqualsTheStoredKey) {
    referenceIs(QStringLiteral("8A"));
    EXPECT_FALSE(keyColorFor(QStringLiteral("2A")).isValid());
}

TEST_F(KeyMatchCellTest, StrictOnlyDropsLightPurple) {
    referenceIsShifted(QStringLiteral("8A"), QStringLiteral("3A"));
    setHighlight(Highlight::StrictOnly);
    EXPECT_EQ(QVariant::fromValue(kPurple), keyColorFor(QStringLiteral("2A")));
    EXPECT_FALSE(keyColorFor(QStringLiteral("5A")).isValid());
}

TEST_F(KeyMatchCellTest, HighlightOffDropsPurple) {
    referenceIsShifted(QStringLiteral("8A"), QStringLiteral("3A"));
    setHighlight(Highlight::Off);
    EXPECT_FALSE(keyColorFor(QStringLiteral("2A")).isValid());
}

TEST_F(KeyMatchCellTest, TurningShiftedMatchingOffDropsPurpleAndRepaints) {
    referenceIsShifted(QStringLiteral("8A"), QStringLiteral("3A"));
    ASSERT_EQ(QVariant::fromValue(kPurple), keyColorFor(QStringLiteral("2A")));
    QSignalSpy repainted(m_pModel.get(), &QAbstractItemModel::dataChanged);
    setMatchShiftedKey(false);
    EXPECT_GE(repainted.count(), 1);
    EXPECT_FALSE(keyColor().isValid());
}

TEST_F(KeyMatchCellTest, CustomColoursReachTheCellAndRepaint) {
    referenceIsShifted(QStringLiteral("8A"), QStringLiteral("3A"));
    trackKeyIs(QStringLiteral("9A")); // lists the track, so there is a row to repaint
    mixxx::keymatch::Settings settings = mixxx::keymatch::loadSettings(config());
    settings.strictColor = QColor(QStringLiteral("#010101"));
    settings.fuzzyColor = QColor(QStringLiteral("#020202"));
    settings.shiftedStrictColor = QColor(QStringLiteral("#030303"));
    settings.shiftedFuzzyColor = QColor(QStringLiteral("#040404"));
    QSignalSpy repainted(m_pModel.get(), &QAbstractItemModel::dataChanged);
    storeSettings(settings);
    EXPECT_GE(repainted.count(), 1);
    EXPECT_EQ(QVariant::fromValue(QColor(QStringLiteral("#010101"))),
            keyColorFor(QStringLiteral("9A")));
    EXPECT_EQ(QVariant::fromValue(QColor(QStringLiteral("#020202"))),
            keyColorFor(QStringLiteral("10A")));
    EXPECT_EQ(QVariant::fromValue(QColor(QStringLiteral("#030303"))),
            keyColorFor(QStringLiteral("2A")));
    EXPECT_EQ(QVariant::fromValue(QColor(QStringLiteral("#040404"))),
            keyColorFor(QStringLiteral("5A")));
}

TEST_F(KeyMatchCellTest, ChangingTheShiftedKeyRepaints) {
    referenceIs(QStringLiteral("8A"));
    trackKeyIs(QStringLiteral("2A"));
    ASSERT_FALSE(keyColor().isValid());
    QSignalSpy repainted(m_pModel.get(), &QAbstractItemModel::dataChanged);
    m_key.set(KeyUtils::keyToNumericValue(KeyUtils::guessKeyFromText(QStringLiteral("3A"))));
    EXPECT_GE(repainted.count(), 1);
    EXPECT_EQ(QVariant::fromValue(kPurple), keyColor());
}

