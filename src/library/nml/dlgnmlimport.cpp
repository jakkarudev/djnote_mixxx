#include "library/nml/dlgnmlimport.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "library/dao/playlistdao.h"

namespace mixxx {
namespace nml {

QString DlgNmlImport::statusText(NmlTrackStatus status) {
    switch (status) {
    case NmlTrackStatus::Found:
        return tr("found");
    case NmlTrackStatus::FoundNextToNml:
        return tr("found next to the .nml");
    case NmlTrackStatus::NotFound:
        break;
    }
    return tr("missing");
}

DlgNmlImport::DlgNmlImport(
        const NmlImportPlan& plan, const PlaylistDAO& dao, QWidget* parent)
        : QDialog(parent),
          m_dao(dao) {
    setWindowTitle(tr("Import Traktor Playlist"));
    setMinimumSize(560, 420);

    auto* pLayout = new QVBoxLayout(this);

    pLayout->addWidget(new QLabel(tr("Playlist name:"), this));
    m_pNameEdit = new QLineEdit(plan.defaultName, this);
    pLayout->addWidget(m_pNameEdit);
    m_pNameWarning = new QLabel(this);
    pLayout->addWidget(m_pNameWarning);

    auto* pTable = new QTableWidget(plan.tracks.size(), 3, this);
    pTable->setHorizontalHeaderLabels({tr("Title"), tr("Artist"), tr("Status")});
    pTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pTable->setSelectionMode(QAbstractItemView::NoSelection);
    pTable->verticalHeader()->setVisible(false);
    pTable->horizontalHeader()->setStretchLastSection(true);
    for (int row = 0; row < plan.tracks.size(); ++row) {
        const NmlPlanTrack& t = plan.tracks.at(row);
        pTable->setItem(row, 0, new QTableWidgetItem(t.title.isEmpty() ? t.primaryKey : t.title));
        pTable->setItem(row, 1, new QTableWidgetItem(t.artist));
        pTable->setItem(row, 2, new QTableWidgetItem(statusText(t.status)));
    }
    pTable->resizeColumnsToContents();
    pLayout->addWidget(pTable, 1);

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
