#include "library/browse/browsecovercolumn.h"

#include <QDir>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>

#include "library/browse/browsetablemodel.h"
#include "library/covercolumnsort.h"
#include "library/coverlookup.h"
#include "library/tabledelegates/coverartdelegate.h"
#include "moc_browsecovercolumn.cpp"
#include "proto/headers.pb.h"
#include "recording/recordingmanager.h"
#include "widget/wlibrarytableview.h"

namespace {

// Same setting as WTrackTableViewHeader.
const QString kHeaderStateSetting = QStringLiteral("header_state_pb");

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
        m_pSort->sortByOtherColumn();
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

// static
void BrowseCoverColumn::addToSavedHeaderState(
        TrackModel* pTrackModel,
        int column,
        int columnCount,
        int width) {
    const QString saved = pTrackModel->getModelSetting(kHeaderStateSetting);
    if (saved.isEmpty()) {
        // Nothing saved yet: the default header shows the column.
        return;
    }
    const QByteArray array = QByteArray::fromBase64(saved.toLatin1());
    mixxx::library::HeaderViewState state;
    if (!state.ParseFromArray(array.constData(), static_cast<int>(array.size()))) {
        return;
    }
    const int savedColumnCount = state.header_state_size();
    if (savedColumnCount != columnCount - 1) {
        // Already has the cover column, or a layout we don't know.
        return;
    }

    // Columns from the cover column on moved one to the right, and the
    // cover column goes right after the column before it (Preview).
    int coverVisualIndex = savedColumnCount;
    for (int vi = 0; vi < savedColumnCount; ++vi) {
        auto* pHeader = state.mutable_header_state(vi);
        if (pHeader->logical_index() == column - 1) {
            coverVisualIndex = vi + 1;
        } else if (pHeader->logical_index() >= column) {
            pHeader->set_logical_index(pHeader->logical_index() + 1);
        }
    }
    auto* pCoverHeader = state.add_header_state();
    pCoverHeader->set_hidden(false);
    pCoverHeader->set_size(width);
    pCoverHeader->set_logical_index(column);
    for (int vi = savedColumnCount; vi > coverVisualIndex; --vi) {
        state.mutable_header_state()->SwapElements(vi, vi - 1);
    }
    for (int vi = 0; vi < state.header_state_size(); ++vi) {
        state.mutable_header_state(vi)->set_visual_index(vi);
    }
    if (state.sort_indicator_shown() && state.sort_indicator_section() >= column) {
        state.set_sort_indicator_section(state.sort_indicator_section() + 1);
    }

#if GOOGLE_PROTOBUF_VERSION >= 3001000
    const int size = static_cast<int>(state.ByteSizeLong());
#else
    const int size = state.ByteSize();
#endif
    QByteArray migrated(size, '\0');
    state.SerializeToArray(migrated.data(), size);
    pTrackModel->setModelSetting(kHeaderStateSetting, QString(migrated.toBase64()));
}
