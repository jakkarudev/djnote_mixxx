#include "library/nml/dlgnmlimportplaylists.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>


namespace mixxx {
namespace nml {

namespace {
constexpr int kNameColumn = 0;
constexpr int kCountColumn = 1;
} // namespace

DlgNmlImportPlaylists::DlgNmlImportPlaylists(
        const QList<NmlPlaylistChoice>& choices, QWidget* parent)
        : QDialog(parent),
          m_choices(choices) {
    setWindowTitle(tr("Import Traktor Playlists"));
    setMinimumSize(520, 360);

    auto* pLayout = new QVBoxLayout(this);
    pLayout->addWidget(new QLabel(
            tr("This file contains several playlists. Tick the ones to import "
               "and edit their names."),
            this));

    m_pTable = new QTableWidget(m_choices.size(), 2, this);
    m_pTable->setHorizontalHeaderLabels({tr("Playlist"), tr("Tracks")});
    m_pTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_pTable->verticalHeader()->setVisible(false);
    m_pTable->horizontalHeader()->setSectionResizeMode(kNameColumn, QHeaderView::Stretch);
    for (int row = 0; row < m_choices.size(); ++row) {
        const NmlPlaylistChoice& c = m_choices.at(row);
        auto* pName = new QTableWidgetItem(c.name);
        pName->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable |
                Qt::ItemIsUserCheckable);
        pName->setCheckState(c.selected ? Qt::Checked : Qt::Unchecked);
        m_pTable->setItem(row, kNameColumn, pName);
        auto* pCount = new QTableWidgetItem(QString::number(c.trackCount));
        pCount->setFlags(Qt::ItemIsEnabled);
        m_pTable->setItem(row, kCountColumn, pCount);
    }
    pLayout->addWidget(m_pTable, 1);

    m_pWarning = new QLabel(this);
    pLayout->addWidget(m_pWarning);

    m_pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_pButtons->button(QDialogButtonBox::Ok)->setText(tr("Import"));
    connect(m_pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    pLayout->addWidget(m_pButtons);

    connect(m_pTable, &QTableWidget::itemChanged, this, &DlgNmlImportPlaylists::updateOkEnabled);
    updateOkEnabled();
}

QList<NmlPlaylistChoice> DlgNmlImportPlaylists::choices() const {
    QList<NmlPlaylistChoice> result = m_choices;
    for (int row = 0; row < result.size(); ++row) {
        const QTableWidgetItem* pItem = m_pTable->item(row, kNameColumn);
        result[row].name = pItem->text().trimmed();
        result[row].selected = pItem->checkState() == Qt::Checked;
    }
    return result;
}

void DlgNmlImportPlaylists::updateOkEnabled() {
    // A name that is taken is not an error here: Apply adds the numeric suffix.
    QString warning;
    int ticked = 0;
    for (const NmlPlaylistChoice& c : choices()) {
        if (!c.selected) {
            continue;
        }
        ++ticked;
        if (c.name.isEmpty()) {
            warning = tr("Every ticked playlist needs a name.");
        }
    }
    if (ticked == 0) {
        warning = tr("Tick at least one playlist.");
    }
    m_pWarning->setText(warning);
    m_pButtons->button(QDialogButtonBox::Ok)->setEnabled(warning.isEmpty());
}

} // namespace nml
} // namespace mixxx

#include "moc_dlgnmlimportplaylists.cpp"
