#include "library/browse/browsecovercolumn.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QHeaderView>
#include <QSignalSpy>
#include <QStringList>
#include <QTableView>
#include <QTemporaryDir>
#include <memory>
#include <string>

#include "library/browse/browsetablemodel.h"
#include "library/coverlookup.h"
#include "library/dao/settingsdao.h"
#include "library/proxytrackmodel.h"
#include "mixer/playerinfo.h"
#include "proto/headers.pb.h"
#include "test/librarytest.h"
#include "util/fileaccess.h"
#include "widget/wtracktableviewheader.h"

namespace {

// Logical indexes of the columns as saved by Mixxx (and DJNote before the
// cover column).
constexpr int kPreview = 0;
constexpr int kFilename = 1;
constexpr int kArtist = 2;
constexpr int kTitle = 3;
constexpr int kAlbum = 4;
constexpr int kYear = 6;
constexpr int kReplayGain = 20;
constexpr int kUpstreamColumnCount = 21;

// Hidden by default in Computer.
bool hiddenByDefault(int upstreamColumn) {
    // Track #, Year, Composer, Location, Album Artist, Grouping,
    // File Created, ReplayGain
    return QList<int>{5, 6, 8, 15, 16, 17, 19, 20}.contains(upstreamColumn);
}

/// The columns Computer shows by default, in Mixxx's order.
const std::string kDefaultColumnsAfterPreview =
        "Filename|Artist|Title|Album|Genre|Comment|Duration|BPM|Key|Type|"
        "Bitrate|File Modified";

class BrowseCoverColumnTest : public LibraryTest {
  protected:
    BrowseCoverColumnTest()
            : m_coverLookup(internalCollection()) {
        PlayerInfo::create();
    }
    ~BrowseCoverColumnTest() override {
        PlayerInfo::destroy();
    }

    std::unique_ptr<BrowseTableModel> browseModel() {
        return std::make_unique<BrowseTableModel>(
                nullptr, trackCollectionManager(), nullptr, &m_coverLookup);
    }

    /// A header layout saved by WTrackTableViewHeader: (logical index,
    /// hidden) in visual order, without column names (Computer has none).
    void saveLayout(const QList<std::pair<int, bool>>& columns,
            int sortSection,
            Qt::SortOrder sortOrder) {
        mixxx::library::HeaderViewState state;
        int visualIndex = 0;
        for (const auto& [logicalIndex, hidden] : columns) {
            auto* pHeader = state.add_header_state();
            pHeader->set_hidden(hidden);
            pHeader->set_size(80);
            pHeader->set_logical_index(logicalIndex);
            pHeader->set_visual_index(visualIndex++);
        }
        state.set_sort_indicator_shown(true);
        state.set_sort_indicator_section(sortSection);
        state.set_sort_order(sortOrder);
        const std::string bytes = state.SerializeAsString();
        SettingsDAO(internalCollection()->database())
                .setValue(QStringLiteral("mixxx.db.model.browse.header_state_pb"),
                        QString(QByteArray(bytes.data(), static_cast<int>(bytes.size()))
                                        .toBase64()));
    }

    /// Mixxx's default layout, changed by the DJ: Title moved before Artist,
    /// Album hidden and Year shown, sorted by Title.
    void saveUpstreamLayoutChangedByTheDj() {
        QList<std::pair<int, bool>> columns{{kPreview, false},
                {kFilename, false},
                {kTitle, false},
                {kArtist, false},
                {kAlbum, true}};
        for (int column = kAlbum + 1; column < kUpstreamColumnCount; ++column) {
            columns.append({column, column != kYear && hiddenByDefault(column)});
        }
        saveLayout(columns, kTitle, Qt::DescendingOrder);
    }

    /// What the DJ sees in the header that shows `pModel`.
    struct Header {
        /// The visible columns' labels in visual order, e.g. "Preview|Filename".
        std::string visibleColumns;
        std::string sortColumn;
        Qt::SortOrder sortOrder;
    };
    static Header header(BrowseTableModel* pModel) {
        WTrackTableViewHeader header(Qt::Horizontal, nullptr);
        header.setModel(pModel);
        QStringList labels;
        for (int visualIndex = 0; visualIndex < header.count(); ++visualIndex) {
            const int logicalIndex = header.logicalIndex(visualIndex);
            if (!header.isSectionHidden(logicalIndex)) {
                labels.append(pModel->headerData(logicalIndex, Qt::Horizontal).toString());
            }
        }
        const QString sortColumn = header.sortIndicatorSection() < 0
                ? QString()
                : pModel->headerData(header.sortIndicatorSection(), Qt::Horizontal)
                          .toString();
        return Header{labels.join('|').toStdString(),
                sortColumn.toStdString(),
                header.sortIndicatorOrder()};
    }

    /// Shows `folder`, which has `fileCount` files, in `pModel` (as Computer
    /// does) and waits until all its files are listed.
    static void showFolder(BrowseTableModel* pModel, const QTemporaryDir& folder, int fileCount) {
        QSignalSpy rowsInserted(pModel, &QAbstractItemModel::rowsInserted);
        pModel->setPath(mixxx::FileAccess(mixxx::FileInfo(folder.path())));
        while (pModel->rowCount() < fileCount) {
            if (!rowsInserted.wait(kTimeoutMillis)) {
                ADD_FAILURE() << "Folder not listed";
                return;
            }
        }
    }

    /// Sorts as WTrackTableView does when the DJ clicks a header: the
    /// header shows the new sort, then the model is asked to sort.
    static void clickToSort(QTableView* pView, int column, Qt::SortOrder order) {
        pView->horizontalHeader()->setSortIndicator(column, order);
        pView->model()->sort(column, order);
    }

    static bool headerShowsReading(const BrowseTableModel& model) {
        return model.headerData(COLUMN_COVERART, Qt::Horizontal)
                .toString()
                .contains(QStringLiteral("reading"));
    }

    QString testFile(const QString& relativePath) const {
        return getTestDir().filePath(relativePath);
    }

    static constexpr int kTimeoutMillis = 5000;

    CoverLookup m_coverLookup;
};

TEST_F(BrowseCoverColumnTest, newHeaderShowsCoverColumnRightAfterPreview) {
    const auto pModel = browseModel();

    EXPECT_EQ("Preview|Cover Art|" + kDefaultColumnsAfterPreview,
            header(pModel.get()).visibleColumns);
}

TEST_F(BrowseCoverColumnTest, headerSavedBeforeCoverColumnShowsItAfterPreview) {
    saveUpstreamLayoutChangedByTheDj();

    const auto pModel = browseModel();

    const Header shown = header(pModel.get());
    EXPECT_EQ("Preview|Cover Art|Filename|Title|Artist|Year|Genre|"
              "Comment|Duration|BPM|Key|Type|Bitrate|File Modified",
            shown.visibleColumns);
    EXPECT_EQ("Title", shown.sortColumn);
    EXPECT_EQ(Qt::DescendingOrder, shown.sortOrder);
}

TEST_F(BrowseCoverColumnTest, headerSavedByEarlierCoverColumnBuildsKeepsItsLayout) {
    // Earlier DJNote builds put the cover column at 1 and moved the others
    // one to the right: Preview(0) Cover(1) Filename(2) Artist(3) Title(4).
    QList<std::pair<int, bool>> columns{{0, false},
            {1, false},
            {2, false},
            {4, false},
            {3, false},
            {5, true}};
    for (int column = kAlbum + 1; column < kUpstreamColumnCount; ++column) {
        columns.append({column + 1, column != kYear && hiddenByDefault(column)});
    }
    saveLayout(columns, 4, Qt::DescendingOrder);

    const auto pModel = browseModel();

    const Header shown = header(pModel.get());
    EXPECT_EQ("Preview|Cover Art|Filename|Title|Artist|Year|Genre|"
              "Comment|Duration|BPM|Key|Type|Bitrate|File Modified",
            shown.visibleColumns);
    EXPECT_EQ("Title", shown.sortColumn);
}

TEST_F(BrowseCoverColumnTest, layoutSavedWithCoverColumnIsRestored) {
    saveUpstreamLayoutChangedByTheDj();
    {
        const auto pModel = browseModel();
        WTrackTableViewHeader shown(Qt::Horizontal, nullptr);
        shown.setModel(pModel.get());
        // The DJ moves the cover column to the end and shows ReplayGain.
        shown.moveSection(shown.visualIndex(COLUMN_COVERART), shown.count() - 1);
        shown.showSection(COLUMN_REPLAYGAIN);
        shown.saveHeaderState();
    }

    const auto pModel = browseModel();

    EXPECT_EQ("Preview|Filename|Title|Artist|Year|Genre|"
              "Comment|Duration|BPM|Key|Type|Bitrate|File Modified|"
              "ReplayGain|Cover Art",
            header(pModel.get()).visibleColumns);
}

TEST_F(BrowseCoverColumnTest, cancellingCoverSortPutsSortIndicatorBackOnTheListsSort) {
    QTemporaryDir folder;
    ASSERT_TRUE(folder.isValid());
    for (const QString& name : {QStringLiteral("a.mp3"), QStringLiteral("b.mp3")}) {
        ASSERT_TRUE(QFile::copy(testFile(QStringLiteral("id3-test-data/cover-test-png.mp3")),
                folder.filePath(name)));
    }
    const auto pModel = browseModel();
    // As Computer shows it.
    ProxyTrackModel proxy(pModel.get(), true);
    proxy.setSortRole(Qt::UserRole);
    QTableView view;
    view.setModel(&proxy);
    showFolder(pModel.get(), folder, 2);
    clickToSort(&view, COLUMN_FILENAME, Qt::DescendingOrder);

    clickToSort(&view, COLUMN_COVERART, Qt::AscendingOrder);
    ASSERT_TRUE(headerShowsReading(*pModel));
    clickToSort(&view, COLUMN_COVERART, Qt::DescendingOrder);
    QCoreApplication::processEvents();

    // The list is still sorted by file name, and the header says so.
    EXPECT_EQ(COLUMN_FILENAME, view.horizontalHeader()->sortIndicatorSection());
    EXPECT_EQ(Qt::DescendingOrder, view.horizontalHeader()->sortIndicatorOrder());
    EXPECT_EQ(QStringLiteral("b.mp3"),
            proxy.index(0, COLUMN_FILENAME).data().toString());
    // Re-applying the header's sort (e.g. on the next folder) reads nothing.
    proxy.sort(view.horizontalHeader()->sortIndicatorSection(),
            view.horizontalHeader()->sortIndicatorOrder());
    EXPECT_FALSE(headerShowsReading(*pModel));
}

TEST_F(BrowseCoverColumnTest, upstreamColumnsKeepTheirIndexes) {
    EXPECT_EQ(kPreview, COLUMN_PREVIEW);
    EXPECT_EQ(kTitle, COLUMN_TITLE);
    EXPECT_EQ(kReplayGain, COLUMN_REPLAYGAIN);
    EXPECT_EQ(kUpstreamColumnCount, COLUMN_COVERART);
}

} // namespace
