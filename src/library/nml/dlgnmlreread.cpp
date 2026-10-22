#include "library/nml/dlgnmlreread.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "library/nml/nmlreviewwidgets.h"

namespace mixxx {
namespace nml {

DlgNmlReread::DlgNmlReread(const NmlRereadPlan& plan,
        const QString& playlistName,
        TrackCollectionManager* pManager,
        QWidget* parent)
        : QDialog(parent),
          m_plan(plan),
          m_pManager(pManager) {
    setWindowTitle(tr("Re-read NML"));
    setMinimumSize(560, 460);
    auto* pLayout = new QVBoxLayout(this);

    pLayout->addWidget(new QLabel(
            tr("Playlist \"%1\" is read again from:\n%2")
                    .arg(playlistName, m_plan.plan.nmlPath),
            this));

    m_pReplace = new QRadioButton(
            tr("Replace the list: the playlist's tracks and order become the .nml's current list"),
            this);
    m_pKeep = new QRadioButton(
            tr("Keep the list: only look for missing tracks again and fill in their preparation"),
            this);
    m_pKeep->setChecked(true);
    pLayout->addWidget(m_pReplace);
    pLayout->addWidget(m_pKeep);

    m_pTable = new QTableWidget(0, 3, this);
    m_pTable->setHorizontalHeaderLabels({tr("Title"), tr("Artist"), tr("Status")});
    m_pTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_pTable->verticalHeader()->setVisible(false);
    connect(m_pTable, &QTableWidget::itemChanged, this, &DlgNmlReread::onItemChanged);
    m_pTable->horizontalHeader()->setStretchLastSection(true);
    pLayout->addWidget(m_pTable, 1);

    auto* pPanels = new NmlReviewPanels(
            [this]() { return QList<NmlImportPlan*>{&m_plan.plan}; },
            m_pManager,
            QFileInfo(m_plan.plan.nmlPath).absolutePath(),
            this);
    connect(pPanels, &NmlReviewPanels::changed, this, &DlgNmlReread::refreshRows);
    pLayout->addWidget(pPanels);
    refreshRows();

    auto* pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    pButtons->button(QDialogButtonBox::Ok)->setText(tr("Re-read"));
    connect(pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    pLayout->addWidget(pButtons);
}

NmlRereadMode DlgNmlReread::mode() const {
    return m_pReplace->isChecked() ? NmlRereadMode::Replace : NmlRereadMode::Keep;
}

void DlgNmlReread::refreshRows() {
    m_refreshing = true;
    const auto& tracks = m_plan.plan.tracks;
    m_pTable->setRowCount(tracks.size());
    for (int row = 0; row < tracks.size(); ++row) {
        const NmlPlanTrack& t = tracks.at(row);
        m_pTable->setItem(row, 0, new QTableWidgetItem(t.title.isEmpty() ? t.primaryKey : t.title));
        m_pTable->setItem(row, 1, new QTableWidgetItem(t.artist));
        m_pTable->setItem(row, 2, newNmlStatusItem(t));
    }
    m_pTable->resizeColumnsToContents();
    m_refreshing = false;
}

void DlgNmlReread::onItemChanged(QTableWidgetItem* pItem) {
    if (m_refreshing || pItem->column() != 2) {
        return;
    }
    applyNmlStatusTick(&m_plan.plan, pItem->row(), pItem->checkState() == Qt::Checked);
    if (pItem->row() < m_plan.plan.tracks.size()) {
        m_refreshing = true;
        pItem->setText(nmlRowStatusText(m_plan.plan.tracks.at(pItem->row())));
        m_refreshing = false;
    }
}

} // namespace nml
} // namespace mixxx

#include "moc_dlgnmlreread.cpp"
