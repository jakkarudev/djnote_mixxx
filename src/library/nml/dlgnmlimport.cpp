#include "library/nml/dlgnmlimport.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QFileInfo>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlreviewwidgets.h"

namespace mixxx {
namespace nml {

DlgNmlImport::DlgNmlImport(const NmlImportPlan& plan,
        const PlaylistDAO& dao,
        QWidget* parent,
        TrackCollectionManager* pManager)
        : QDialog(parent),
          m_plan(plan),
          m_dao(dao),
          m_pManager(pManager) {
    setWindowTitle(tr("Import NML"));
    setMinimumSize(560, 420);

    auto* pLayout = new QVBoxLayout(this);

    pLayout->addWidget(new QLabel(tr("Playlist name:"), this));
    m_pNameEdit = new QLineEdit(plan.defaultName, this);
    pLayout->addWidget(m_pNameEdit);
    m_pNameWarning = new QLabel(this);
    pLayout->addWidget(m_pNameWarning);

    m_pTable = new QTableWidget(0, 3, this);
    m_pTable->setHorizontalHeaderLabels({tr("Title"), tr("Artist"), tr("Status")});
    m_pTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_pTable->verticalHeader()->setVisible(false);
    m_pTable->horizontalHeader()->setStretchLastSection(true);
    connect(m_pTable, &QTableWidget::itemChanged, this, &DlgNmlImport::onItemChanged);
    pLayout->addWidget(m_pTable, 1);

    // DJNote: act on the rows marked missing, updating them in place.
    auto* pPanels = new NmlReviewPanels(
            [this]() { return QList<NmlImportPlan*>{&m_plan}; },
            m_pManager,
            QFileInfo(m_plan.nmlPath).absolutePath(),
            this);
    connect(pPanels, &NmlReviewPanels::changed, this, &DlgNmlImport::refreshRows);
    pLayout->addWidget(pPanels);
    refreshRows();

    m_pButtons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_pButtons->button(QDialogButtonBox::Ok)->setText(tr("Import"));
    connect(m_pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    pLayout->addWidget(m_pButtons);

    connect(m_pNameEdit, &QLineEdit::textChanged, this, &DlgNmlImport::updateOkEnabled);
    updateOkEnabled();
}

QString DlgNmlImport::playlistName() const {
    return m_pNameEdit->text().trimmed();
}

void DlgNmlImport::refreshRows() {
    m_refreshing = true;
    m_pTable->setRowCount(m_plan.tracks.size());
    for (int row = 0; row < m_plan.tracks.size(); ++row) {
        const NmlPlanTrack& t = m_plan.tracks.at(row);
        m_pTable->setItem(row, 0, new QTableWidgetItem(t.title.isEmpty() ? t.primaryKey : t.title));
        m_pTable->setItem(row, 1, new QTableWidgetItem(t.artist));
        m_pTable->setItem(row, 2, newNmlStatusItem(t));
    }
    m_pTable->resizeColumnsToContents();
    m_refreshing = false;
}

void DlgNmlImport::onItemChanged(QTableWidgetItem* pItem) {
    if (m_refreshing || pItem->column() != 2) {
        return;
    }
    applyNmlStatusTick(&m_plan, pItem->row(), pItem->checkState() == Qt::Checked);
    // The label of an unticked reuse row changes (kept separate).
    if (pItem->row() < m_plan.tracks.size()) {
        m_refreshing = true;
        pItem->setText(nmlRowStatusText(m_plan.tracks.at(pItem->row())));
        m_refreshing = false;
    }
}

void DlgNmlImport::updateOkEnabled() {
    const QString name = playlistName();
    QString warning;
    if (name.isEmpty()) {
        warning = tr("Enter a playlist name.");
    } else if (m_dao.getPlaylistIdFromName(name) != kInvalidPlaylistId) {
        warning = tr("A playlist with this name already exists.");
    }
    m_pNameWarning->setText(warning);
    m_pButtons->button(QDialogButtonBox::Ok)->setEnabled(warning.isEmpty());
}

} // namespace nml
} // namespace mixxx

#include "moc_dlgnmlimport.cpp"
