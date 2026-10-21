#include "library/externalcovercolumn.h"

#include "library/basesqltablemodel.h"
#include "library/coverlookup.h"
#include "library/dao/trackschema.h"
#include "moc_externalcovercolumn.cpp"
#include "proto/headers.pb.h"
#include "util/assert.h"

namespace {

// Same setting as WTrackTableViewHeader.
const QString kHeaderStateSetting = QStringLiteral("header_state_pb");

} // namespace

// static
QString ExternalCoverColumn::viewColumn() {
    return QStringLiteral("'' AS ") + LIBRARYTABLE_COVERART;
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

ExternalCoverColumn::ExternalCoverColumn(BaseSqlTableModel* pModel)
        : QObject(pModel),
          m_pModel(pModel) {
    // Rows are replaced on every select() (search, sort, refresh).
    const auto clearRows = [this] {
        m_rowsByLocation.clear();
    };
    connect(m_pModel, &QAbstractItemModel::rowsRemoved, this, clearRows);
    connect(m_pModel, &QAbstractItemModel::modelReset, this, clearRows);
    connect(m_pModel, &QAbstractItemModel::layoutChanged, this, clearRows);
}

void ExternalCoverColumn::setCoverLookup(CoverLookup* pCoverLookup) {
    if (m_pCoverLookup == pCoverLookup) {
        return;
    }
    if (m_pCoverLookup) {
        disconnect(m_pCoverLookup, nullptr, this, nullptr);
    }
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

void ExternalCoverColumn::addToSavedHeaderState() {
    const int column = m_pModel->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_COVERART);
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
