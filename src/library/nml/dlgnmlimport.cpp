#include "library/nml/dlgnmlimport.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlfindmissing.h"

namespace mixxx {
namespace nml {

QString DlgNmlImport::statusText(NmlTrackStatus status) {
    switch (status) {
    case NmlTrackStatus::Found:
        return tr("found");
    case NmlTrackStatus::FoundNextToNml:
        return tr("found next to the .nml");
    case NmlTrackStatus::FoundInBrowsedFolder:
        return tr("found in browsed folder");
    case NmlTrackStatus::MatchedInLibrary:
        return tr("matched in library");
    case NmlTrackStatus::NotFound:
        break;
    }
    return tr("missing");
}

DlgNmlImport::DlgNmlImport(const NmlImportPlan& plan,
        const PlaylistDAO& dao,
        QWidget* parent,
        TrackCollectionManager* pManager)
        : QDialog(parent),
          m_plan(plan),
          m_dao(dao),
          m_pManager(pManager) {
    setWindowTitle(tr("Import Traktor Playlist"));
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

    // DJNote (#111): act on the rows marked missing, updating them in place.
    auto* pFindRow = new QHBoxLayout();
    auto* pBrowse = new QPushButton(tr("Browse for the moved folder…"), this);
    connect(pBrowse, &QPushButton::clicked, this, &DlgNmlImport::browseForMovedFolder);
    pFindRow->addWidget(pBrowse);
    if (m_pManager) {
        auto* pFind = new QPushButton(tr("Find in library"), this);
        connect(pFind, &QPushButton::clicked, this, &DlgNmlImport::findInLibrary);
        pFindRow->addWidget(pFind);
    }
    pFindRow->addStretch(1);
    pLayout->addLayout(pFindRow);
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
        auto* pStatus = new QTableWidgetItem(statusText(t.status));
        if (t.status == NmlTrackStatus::MatchedInLibrary) {
            pStatus->setFlags(pStatus->flags() | Qt::ItemIsUserCheckable);
            pStatus->setCheckState(t.accepted ? Qt::Checked : Qt::Unchecked);
        }
        m_pTable->setItem(row, 2, pStatus);
    }
    m_pTable->resizeColumnsToContents();
    m_refreshing = false;
}

void DlgNmlImport::browseForMovedFolder() {
    const QString folder = QFileDialog::getExistingDirectory(
            this, tr("Folder the tracks were moved to"), QFileInfo(m_plan.nmlPath).absolutePath());
    if (folder.isEmpty()) {
        return;
    }
    browseNmlMissingInFolder(&m_plan, folder);
    refreshRows();
}

void DlgNmlImport::findInLibrary() {
    findNmlMissingInLibrary(&m_plan, m_pManager);
    refreshRows();
}

void DlgNmlImport::onItemChanged(QTableWidgetItem* pItem) {
    if (m_refreshing || pItem->column() != 2) {
        return;
    }
    setNmlMatchAccepted(&m_plan, pItem->row(), pItem->checkState() == Qt::Checked);
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
