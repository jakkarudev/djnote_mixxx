#include "library/covercolumnsort.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QHeaderView>
#include <QTableView>
#include <QTimer>
#include <QVariant>

#include "library/coverlookup.h"
#include "library/trackmodel.h"
#include "moc_covercolumnsort.cpp"
#include "util/assert.h"

CoverColumnSort::CoverColumnSort(
        QAbstractItemModel* pModel,
        int column,
        CoverLookup* pCoverLookup,
        QObject* pParent)
        : QObject(pParent),
          m_pModel(pModel),
          m_column(column),
          m_pCoverLookup(pCoverLookup),
          m_sortedByCover(false),
          m_order(Qt::AscendingOrder),
          m_otherColumn(-1),
          m_otherOrder(Qt::AscendingOrder) {
}

CoverColumnSort::~CoverColumnSort() {
    // The batch is cancelled when destroyed. The header goes with the model.
}

bool CoverColumnSort::sortByCover(Qt::SortOrder order, const QStringList& locations) {
    if (isReading() && order != m_order) {
        // The cover header clicked again while reading.
        cancelReading();
        m_sortedByCover = false;
        emit sortCancelled(m_otherColumn, m_otherOrder);
        return false;
    }
    m_sortedByCover = true;
    m_order = order;
    return read(locations);
}

void CoverColumnSort::sortByOtherColumn(int column, Qt::SortOrder order) {
    cancelReading();
    m_sortedByCover = false;
    m_otherColumn = column;
    m_otherOrder = order;
}

bool CoverColumnSort::rowsChanged(const QStringList& locations) {
    VERIFY_OR_DEBUG_ASSERT(m_sortedByCover) {
        return false;
    }
    return read(locations);
}

void CoverColumnSort::rowsCleared() {
    cancelReading();
}

// static
QString CoverColumnSort::sortKey(const QByteArray& imageDigest) {
    return QString::fromLatin1(imageDigest.toHex());
}

// static
void CoverColumnSort::showSortInViews(
        QAbstractItemModel* pViewModel, int column, Qt::SortOrder order) {
    // Not while the view that asked to sort is still sorting.
    QTimer::singleShot(0, pViewModel, [pViewModel, column, order] {
        int sortColumn = column;
        Qt::SortOrder sortOrder = order;
        if (sortColumn < 0) {
            if (auto* pTrackModel = dynamic_cast<TrackModel*>(pViewModel)) {
                sortColumn = pTrackModel->defaultSortColumn();
                sortOrder = pTrackModel->defaultSortOrder();
            }
        }
        // Views don't tell their models about themselves: look for them.
        const auto widgets = QApplication::allWidgets();
        for (QWidget* pWidget : widgets) {
            auto* pView = qobject_cast<QTableView*>(pWidget);
            if (pView && pView->model() == pViewModel) {
                pView->horizontalHeader()->setSortIndicator(sortColumn, sortOrder);
            }
        }
    });
}

bool CoverColumnSort::read(const QStringList& locations) {
    if (!m_pCoverLookup) {
        return true;
    }
    // The new batch is started before the old one is dropped, so the reads
    // they share go on.
    std::unique_ptr<CoverBatch> pBatch = m_pCoverLookup->readCovers(locations);
    if (pBatch->isFinished()) {
        cancelReading();
        return true;
    }
    connect(pBatch.get(), &CoverBatch::progress, this, &CoverColumnSort::slotProgress);
    connect(pBatch.get(), &CoverBatch::finished, this, &CoverColumnSort::slotFinished);
    m_pBatch = std::move(pBatch);
    showProgress(m_pBatch->done(), m_pBatch->total());
    return false;
}

void CoverColumnSort::cancelReading() {
    if (!m_pBatch) {
        return;
    }
    m_pBatch.reset();
    restoreHeader();
}

void CoverColumnSort::slotProgress(int done, int total) {
    if (sender() != m_pBatch.get()) {
        return;
    }
    showProgress(done, total);
}

void CoverColumnSort::slotFinished() {
    if (sender() != m_pBatch.get()) {
        return;
    }
    // Not deleted right away: it is emitting this signal.
    m_pBatch.release()->deleteLater();
    restoreHeader();
    emit readyToSort(m_order);
}

void CoverColumnSort::showProgress(int done, int total) {
    if (!m_pModel) {
        return;
    }
    if (m_headerLabel.isNull()) {
        m_headerLabel = m_pModel->headerData(m_column, Qt::Horizontal).toString();
        m_headerToolTip = m_pModel->headerData(m_column, Qt::Horizontal, Qt::ToolTipRole);
    }
    const QString progress = tr("%1 (reading %2/%3)").arg(m_headerLabel).arg(done).arg(total);
    // DisplayRole, not setHeaderData()'s default EditRole: track table models
    // show a DisplayRole header before an EditRole one.
    m_pModel->setHeaderData(m_column, Qt::Horizontal, progress, Qt::DisplayRole);
    // The column is narrow: the whole text shows in the tooltip.
    m_pModel->setHeaderData(m_column, Qt::Horizontal, progress, Qt::ToolTipRole);
}

void CoverColumnSort::restoreHeader() {
    if (!m_pModel || m_headerLabel.isNull()) {
        return;
    }
    m_pModel->setHeaderData(m_column, Qt::Horizontal, m_headerLabel, Qt::DisplayRole);
    m_pModel->setHeaderData(m_column, Qt::Horizontal, m_headerToolTip, Qt::ToolTipRole);
    m_headerLabel = QString();
    m_headerToolTip = QVariant();
}
