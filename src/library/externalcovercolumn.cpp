#include "library/externalcovercolumn.h"

#include <QSqlQuery>
#include <QTimer>

#include "library/basesqltablemodel.h"
#include "library/covercolumnsort.h"
#include "library/coverlookup.h"
#include "library/dao/trackschema.h"
#include "library/queryutil.h"
#include "moc_externalcovercolumn.cpp"
#include "proto/headers.pb.h"
#include "util/assert.h"
#include "util/db/sqltransaction.h"

namespace {

// Same setting as WTrackTableViewHeader.
const QString kHeaderStateSetting = QStringLiteral("header_state_pb");

// Session-only (temporary) table: track location -> cover sort key
// (CoverColumnSort::sortKey(), empty for no cover).
const QString kSortKeyTable = QStringLiteral("djnote_external_cover_sort_keys");

} // namespace

QString ExternalCoverColumn::viewColumn(const QString& locationExpression) const {
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
                "CREATE TEMPORARY TABLE IF NOT EXISTS %1 "
                "(location TEXT PRIMARY KEY, sort_key TEXT NOT NULL)")
                        .arg(kSortKeyTable))) {
        LOG_FAILED_QUERY(query) << "Error creating the cover sort key table";
    }
    // The keys are by the location the model answers (getTrackLocation()),
    // whose separators are converted as by QDir::fromNativeSeparators().
#if defined(Q_OS_WIN)
    const QString location = QStringLiteral("REPLACE(%1, '\\', '/')").arg(locationExpression);
#else
    const QString& location = locationExpression;
#endif
    return QStringLiteral(
            "(SELECT cover_sort_keys.sort_key FROM %1 AS cover_sort_keys "
            "WHERE cover_sort_keys.location = %2) AS %3")
            .arg(kSortKeyTable, location, LIBRARYTABLE_COVERART);
}

// static
void ExternalCoverColumn::attachCoverLookup(
        QAbstractItemModel* pModel, CoverLookup* pCoverLookup) {
    if (!pModel) {
        return;
    }
    auto* pColumn = pModel->findChild<ExternalCoverColumn*>(
            QString(), Qt::FindDirectChildrenOnly);
    if (pColumn) {
        pColumn->setCoverLookup(pCoverLookup);
    }
}

ExternalCoverColumn::ExternalCoverColumn(
        BaseSqlTableModel* pModel, const QSqlDatabase& database)
        : QObject(pModel),
          m_pModel(pModel),
          m_database(database),
          m_pSort(nullptr),
          m_rowsOfPreviousTable(false),
          m_rowsChangePending(false) {
    // Rows are replaced on every select() (search, sort, refresh).
    const auto clearRows = [this] {
        m_rowsByLocation.clear();
        m_rowsOfPreviousTable = false;
    };
    connect(m_pModel, &QAbstractItemModel::rowsRemoved, this, clearRows);
    connect(m_pModel, &QAbstractItemModel::modelReset, this, clearRows);
    connect(m_pModel, &QAbstractItemModel::layoutChanged, this, clearRows);
    connect(m_pModel, &QAbstractItemModel::rowsInserted, this, [this] {
        m_rowsOfPreviousTable = false;
        // Checked once the select() that inserts them is done.
        if (!m_rowsChangePending) {
            m_rowsChangePending = true;
            QTimer::singleShot(0, this, &ExternalCoverColumn::slotRowsChanged);
        }
    });
}

void ExternalCoverColumn::setCoverLookup(CoverLookup* pCoverLookup) {
    if (m_pCoverLookup == pCoverLookup) {
        return;
    }
    if (m_pCoverLookup) {
        disconnect(m_pCoverLookup, nullptr, this, nullptr);
    }
    delete m_pSort;
    m_pSort = nullptr;
    m_pCoverLookup = pCoverLookup;
    if (m_pCoverLookup) {
        connect(m_pCoverLookup,
                &CoverLookup::coverInfoChanged,
                this,
                &ExternalCoverColumn::slotCoverInfoChanged);
    }
}

CoverInfo ExternalCoverColumn::coverInfo(const QModelIndex& index) const {
    if (!m_pCoverLookup) {
        return CoverInfo();
    }
    const QString location = m_pModel->getTrackLocation(index);
    if (location.isEmpty()) {
        // Lost track
        return CoverInfo();
    }
    QList<int>& rows = m_rowsByLocation[location];
    if (!rows.contains(index.row())) {
        rows.append(index.row());
    }
    return m_pCoverLookup->coverInfo(location);
}

void ExternalCoverColumn::slotCoverInfoChanged(const QString& location) {
    const QList<int> rows = m_rowsByLocation.value(location);
    const int column = m_pModel->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART);
    for (int row : rows) {
        if (row >= m_pModel->rowCount()) {
            continue;
        }
        const QModelIndex index = m_pModel->index(row, column);
        if (m_pModel->getTrackLocation(index) == location) {
            emit m_pModel->dataChanged(index, index);
        }
    }
}

int ExternalCoverColumn::column() const {
    return m_pModel->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART);
}

QStringList ExternalCoverColumn::rowLocations() const {
    QStringList locations;
    const int rowCount = m_pModel->rowCount();
    locations.reserve(rowCount);
    for (int row = 0; row < rowCount; ++row) {
        locations.append(m_pModel->getTrackLocation(m_pModel->index(row, column())));
    }
    return locations;
}

void ExternalCoverColumn::tableSet() {
    addToSavedHeaderState();
    m_rowsOfPreviousTable = m_pModel->rowCount() > 0;
    if (m_pSort) {
        // The covers being read are those of the previous table's rows.
        m_pSort->rowsCleared();
    }
}

bool ExternalCoverColumn::sortRequested(int column, Qt::SortOrder order) {
    if (!m_pCoverLookup) {
        return true;
    }
    if (column != this->column()) {
        if (m_pSort) {
            m_pSort->sortByOtherColumn();
        }
        return true;
    }
    if (!m_pSort) {
        m_pSort = new CoverColumnSort(m_pModel, column, m_pCoverLookup, this);
        connect(m_pSort,
                &CoverColumnSort::readyToSort,
                this,
                &ExternalCoverColumn::slotReadyToSort);
    }
    if (m_rowsOfPreviousTable) {
        // Another playlist: its rows, in the view's order, while its covers
        // are read.
        m_pModel->select();
    }
    const QStringList locations = rowLocations();
    if (!m_pSort->sortByCover(order, locations)) {
        m_readingLocations = QSet<QString>(locations.begin(), locations.end());
        return false;
    }
    writeSortKeys();
    return true;
}

void ExternalCoverColumn::slotReadyToSort(Qt::SortOrder order) {
    m_readingLocations.clear();
    writeSortKeys();
    m_pModel->setSort(column(), order);
    m_pModel->select();
}

void ExternalCoverColumn::slotRowsChanged() {
    m_rowsChangePending = false;
    if (!m_pSort || !m_pSort->isSortedByCover()) {
        return;
    }
    const QStringList locations = rowLocations();
    QSet<QString> locationSet(locations.begin(), locations.end());
    if (m_pSort->isReading() && locationSet == m_readingLocations) {
        // The rows whose covers are being read.
        return;
    }
    if (!m_pSort->rowsChanged(locations)) {
        m_readingLocations = std::move(locationSet);
        return;
    }
    m_readingLocations.clear();
    const QSet<QString> written = writeSortKeys();
    if (written.isEmpty() || written == m_reselectedFor) {
        // Nothing new, or the keys written before didn't reach the rows:
        // don't select() again and again.
        m_reselectedFor.clear();
        return;
    }
    m_reselectedFor = written;
    m_pModel->select();
}

QSet<QString> ExternalCoverColumn::writeSortKeys() {
    QSet<QString> written;
    VERIFY_OR_DEBUG_ASSERT(m_pCoverLookup) {
        return written;
    }
    SqlTransaction transaction(m_database);
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO %1 (location, sort_key) "
            "VALUES (:location, :sort_key)")
                    .arg(kSortKeyTable));
    const int rowCount = m_pModel->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex index = m_pModel->index(row, column());
        const QString location = m_pModel->getTrackLocation(index);
        if (location.isEmpty()) {
            // Lost track
            continue;
        }
        QString sortKey = CoverColumnSort::sortKey(
                m_pCoverLookup->coverInfo(location).imageDigest());
        if (sortKey.isNull()) {
            // No cover. Not NULL, which is "not read yet".
            sortKey = QStringLiteral("");
        }
        // The cover sort value the row was selected with.
        const QVariant rowSortKey = index.data();
        if (!rowSortKey.isNull() && rowSortKey.toString() == sortKey) {
            continue;
        }
        query.bindValue(":location", location);
        query.bindValue(":sort_key", sortKey);
        if (!query.exec()) {
            LOG_FAILED_QUERY(query) << "Error writing a cover sort key";
            continue;
        }
        written.insert(location);
    }
    if (transaction) {
        transaction.commit();
    }
    return written;
}

void ExternalCoverColumn::addToSavedHeaderState() {
    const int column = this->column();
    VERIFY_OR_DEBUG_ASSERT(column >= 0) {
        return;
    }
    const QString saved = m_pModel->getModelSetting(kHeaderStateSetting);
    if (saved.isEmpty()) {
        // Nothing saved yet: the default header shows the column.
        return;
    }
    const QByteArray array = QByteArray::fromBase64(saved.toLatin1());
    mixxx::library::HeaderViewState state;
    if (!state.ParseFromArray(array.constData(), static_cast<int>(array.size()))) {
        return;
    }
    // The header restores columns by name.
    const std::string coverName = LIBRARYTABLE_COVERART.toStdString();
    const std::string previewName = LIBRARYTABLE_PREVIEW.toStdString();
    const int savedColumnCount = state.header_state_size();
    int coverVisualIndex = -1;
    for (int vi = 0; vi < savedColumnCount; ++vi) {
        const auto& header = state.header_state(vi);
        if (header.column_name() == coverName) {
            // Already has the column, shown or hidden by the DJ.
            return;
        }
        if (header.column_name() == previewName) {
            coverVisualIndex = vi + 1;
        }
    }
    if (coverVisualIndex < 0) {
        // A layout we don't know.
        return;
    }

    // Columns from the cover column on moved one to the right.
    for (int vi = 0; vi < savedColumnCount; ++vi) {
        auto* pHeader = state.mutable_header_state(vi);
        if (pHeader->logical_index() >= column) {
            pHeader->set_logical_index(pHeader->logical_index() + 1);
        }
    }
    auto* pCoverHeader = state.add_header_state();
    pCoverHeader->set_hidden(false);
    pCoverHeader->set_size(
            m_pModel->headerData(column, Qt::Horizontal, TrackModel::kHeaderWidthRole)
                    .toInt());
    pCoverHeader->set_logical_index(column);
    pCoverHeader->set_column_name(coverName);
    for (int vi = savedColumnCount; vi > coverVisualIndex; --vi) {
        state.mutable_header_state()->SwapElements(vi, vi - 1);
    }
    for (int vi = 0; vi < state.header_state_size(); ++vi) {
        state.mutable_header_state(vi)->set_visual_index(vi);
    }
    if (state.sort_indicator_shown() && state.sort_indicator_section() >= column) {
        state.set_sort_indicator_section(state.sort_indicator_section() + 1);
    }

    const std::string bytes = state.SerializeAsString();
    m_pModel->setModelSetting(kHeaderStateSetting,
            QString(QByteArray(bytes.data(), static_cast<int>(bytes.size())).toBase64()));
}
