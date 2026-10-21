#include "library/browse/browsecovercolumn.h"

#include <QApplication>
#include <QDir>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStyle>

#include "library/browse/browsetablemodel.h"
#include "library/columncache.h"
#include "library/covercolumnsort.h"
#include "library/coverlookup.h"
#include "library/dao/trackschema.h"
#include "library/savedheaderstate.h"
#include "library/tabledelegates/coverartdelegate.h"
#include "moc_browsecovercolumn.cpp"
#include "recording/recordingmanager.h"
#include "widget/wlibrarytableview.h"

namespace {

// Earlier builds had the cover column at 1, right after Preview, and the
// upstream columns from Filename on one to the right.
constexpr int kEarlierBuildsCoverColumn = 1;
constexpr int kEarlierBuildsColumnCount = 22;

} // namespace

BrowseCoverColumn::BrowseCoverColumn(
        BrowseTableModel* pModel,
        int column,
        CoverLookup* pCoverLookup,
        RecordingManager* pRecordingManager)
        : QObject(pModel),
          m_pModel(pModel),
          m_column(column),
          m_pCoverLookup(pCoverLookup),
          m_pRecordingManager(pRecordingManager),
          m_pSort(new CoverColumnSort(pModel, column, pCoverLookup, this)) {
    if (m_pCoverLookup) {
        connect(m_pCoverLookup,
                &CoverLookup::coverInfoChanged,
                this,
                &BrowseCoverColumn::slotCoverInfoChanged);
    }
    connect(m_pSort,
            &CoverColumnSort::readyToSort,
            this,
            &BrowseCoverColumn::slotReadyToSort);
    connect(m_pSort,
            &CoverColumnSort::sortCancelled,
            this,
            [this](int column, Qt::SortOrder order) {
                if (m_pProxy) {
                    CoverColumnSort::showSortInViews(m_pProxy, column, order);
                }
            });
    m_rowsInsertedTimer.setSingleShot(true);
    m_rowsInsertedTimer.setInterval(0);
    connect(&m_rowsInsertedTimer,
            &QTimer::timeout,
            this,
            &BrowseCoverColumn::slotRowsInserted);
    connect(m_pModel,
            &QAbstractItemModel::rowsInserted,
            this,
            [this] {
                if (m_pSort->isSortedByCover()) {
                    m_rowsInsertedTimer.start();
                }
            });
}

QString BrowseCoverColumn::coverLocation(const QModelIndex& index) const {
    const QString location = m_pModel->getTrackLocation(index);
    // The file being recorded is still written: don't read it, and never
    // resolve it as a track (that pops up a "file in use" dialog).
    if (location.isEmpty() ||
            (m_pRecordingManager &&
                    location ==
                            QDir::fromNativeSeparators(
                                    m_pRecordingManager->getRecordingLocation()))) {
        return QString();
    }
    return location;
}

QStringList BrowseCoverColumn::coverLocations() const {
    QStringList locations;
    const int rowCount = m_pModel->rowCount();
    locations.reserve(rowCount);
    for (int row = 0; row < rowCount; ++row) {
        locations.append(coverLocation(m_pModel->index(row, m_column)));
    }
    return locations;
}

CoverInfo BrowseCoverColumn::coverInfo(const QModelIndex& index) const {
    if (!m_pCoverLookup) {
        return CoverInfo();
    }
    const QString location = coverLocation(index);
    if (location.isEmpty()) {
        return CoverInfo();
    }
    m_rowByLocation.insert(location, index.row());
    return m_pCoverLookup->coverInfo(location);
}

QAbstractItemDelegate* BrowseCoverColumn::createDelegate(QObject* pParent) const {
    auto* pTableView = qobject_cast<WLibraryTableView*>(pParent);
    VERIFY_OR_DEBUG_ASSERT(pTableView) {
        return nullptr;
    }
    auto* pDelegate = new CoverArtDelegate(pTableView);
    connect(pTableView,
            &WLibraryTableView::onlyCachedCoverArt,
            pDelegate,
            &CoverArtDelegate::slotInhibitLazyLoading);
    // The delegate's rows are the view's rows, which the view's sort
    // proxy may have reordered: repaint them in the view.
    connect(pDelegate,
            &CoverArtDelegate::rowsChanged,
            pTableView,
            [pTableView, column = m_column](const QList<int>& rows) {
                for (int row : rows) {
                    pTableView->update(pTableView->model()->index(row, column));
                }
            });
    return pDelegate;
}

void BrowseCoverColumn::clearRows() {
    m_rowByLocation.clear();
    m_rowsInsertedTimer.stop();
    m_pSort->rowsCleared();
}

bool BrowseCoverColumn::sortRequested(
        QSortFilterProxyModel* pProxy, int column, Qt::SortOrder order) {
    m_pProxy = pProxy;
    if (column != m_column) {
        m_pSort->sortByOtherColumn(column, order);
        return true;
    }
    m_rowsInsertedTimer.stop();
    if (!m_pSort->sortByCover(order, coverLocations())) {
        // The list keeps its order until the covers are read.
        return false;
    }
    writeSortKeys();
    return true;
}

void BrowseCoverColumn::slotRowsInserted() {
    if (!m_pSort->isSortedByCover()) {
        return;
    }
    if (m_pSort->rowsChanged(coverLocations())) {
        slotReadyToSort(m_pSort->sortOrder());
    }
}

void BrowseCoverColumn::slotReadyToSort(Qt::SortOrder order) {
    if (!m_pProxy) {
        return;
    }
    writeSortKeys();
    // Not through the proxy's sort(), which would ask again.
    m_pProxy->QSortFilterProxyModel::sort(m_column, order);
}

void BrowseCoverColumn::writeSortKeys() {
    if (!m_pProxy || !m_pCoverLookup) {
        return;
    }
    const int sortRole = m_pProxy->sortRole();
    {
        // All at once: the proxy must not re-sort row by row.
        const QSignalBlocker blocker(m_pModel);
        const int rowCount = m_pModel->rowCount();
        for (int row = 0; row < rowCount; ++row) {
            QStandardItem* pItem = m_pModel->item(row, m_column);
            if (!pItem) {
                continue;
            }
            const QString location = coverLocation(m_pModel->index(row, m_column));
            const QByteArray digest = location.isEmpty()
                    ? QByteArray()
                    : m_pCoverLookup->coverInfo(location).imageDigest();
            pItem->setData(CoverColumnSort::sortKey(digest), sortRole);
        }
    }
}

void BrowseCoverColumn::slotCoverInfoChanged(const QString& location) {
    const int row = m_rowByLocation.value(location, -1);
    if (row < 0 || row >= m_pModel->rowCount()) {
        return;
    }
    const QModelIndex index = m_pModel->index(row, m_column);
    if (m_pModel->getTrackLocation(index) != location) {
        return;
    }
    emit m_pModel->dataChanged(index, index);
}

void BrowseCoverColumn::addToHeader() {
    // Same default width as the cover column in Tracks.
    const int width = ColumnCache::defaultColumnWidth() / 2;
    m_pModel->setHeaderData(m_column, Qt::Horizontal, width, TrackModel::kHeaderWidthRole);
    // The header restores a named column by its name, the others (unnamed in
    // Computer) by their index.
    m_pModel->setHeaderData(m_column,
            Qt::Horizontal,
            LIBRARYTABLE_COVERART,
            TrackModel::kHeaderNameRole);
    addToSavedHeaderState(width);
}

void BrowseCoverColumn::addToSavedHeaderState(int width) {
    SavedHeaderState header(m_pModel);
    if (header.visualIndexOfName(LIBRARYTABLE_COVERART) >= 0) {
        // Saved with the cover column, shown or hidden by the DJ.
        return;
    }
    if (!header.isSaved()) {
        // Nothing saved yet: the header as Mixxx shows it by default, which
        // hides some columns but lists all of them in the model's order.
        const int defaultWidth = QApplication::style()->pixelMetric(
                QStyle::PM_HeaderDefaultSectionSizeHorizontal);
        for (int column = 0; column < m_pModel->columnCount(); ++column) {
            if (column == m_column) {
                continue;
            }
            const int columnWidth =
                    m_pModel->headerData(column, Qt::Horizontal, TrackModel::kHeaderWidthRole)
                            .toInt();
            header.appendColumn(column,
                    columnWidth > 0 ? columnWidth : defaultWidth,
                    m_pModel->isColumnHiddenByDefault(column));
        }
    } else if (header.columnCount() == kEarlierBuildsColumnCount) {
        // Saved by an earlier build: the same layout with the columns'
        // current indexes.
        header.moveColumn(kEarlierBuildsCoverColumn, m_column);
        header.setName(header.visualIndexOfLogicalIndex(m_column), LIBRARYTABLE_COVERART);
        header.save();
        return;
    } else if (header.columnCount() > m_column) {
        // A layout we don't know.
        return;
    }
    // Saved before the cover column existed (or nothing saved): add it right
    // after Preview.
    header.insertColumn(header.visualIndexOfLogicalIndex(COLUMN_PREVIEW) + 1,
            m_column,
            width,
            LIBRARYTABLE_COVERART);
    header.save();
}
