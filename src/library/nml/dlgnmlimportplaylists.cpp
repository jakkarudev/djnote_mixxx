#include "library/nml/dlgnmlimportplaylists.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "library/nml/nmlreviewwidgets.h"
#include "library/nml/nmlreuse.h"

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
    init(nullptr);
}

DlgNmlImportPlaylists::DlgNmlImportPlaylists(const QList<NmlPlaylistChoice>& choices,
        const NmlImportPlan& plan,
        TrackCollectionManager* pManager,
        QWidget* parent)
        : QDialog(parent),
          m_choices(choices),
          m_plans(nmlPlansForChoices(plan, choices)) {
    init(pManager);
}

void DlgNmlImportPlaylists::init(TrackCollectionManager* pManager) {
    setWindowTitle(tr("Import NML"));
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

    if (pManager) {
        for (NmlImportPlan& plan : m_plans) {
            proposeNmlReuse(&plan, pManager);
        }
    }
    if (!m_plans.isEmpty()) {
        // Missing / matched / reused rows of the ticked playlists.
        m_pRows = new QTableWidget(0, 4, this);
        m_pRows->setHorizontalHeaderLabels({tr("Playlist"), tr("Title"), tr("Artist"), tr("Status")});
        m_pRows->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_pRows->setSelectionMode(QAbstractItemView::NoSelection);
        m_pRows->verticalHeader()->setVisible(false);
        m_pRows->horizontalHeader()->setStretchLastSection(true);
        connect(m_pRows, &QTableWidget::itemChanged, this, &DlgNmlImportPlaylists::onRowChanged);
        pLayout->addWidget(new QLabel(tr("Tracks not found at their recorded path:"), this));
        pLayout->addWidget(m_pRows, 1);
        auto* pPanels = new NmlReviewPanels([this]() { return tickedPlans(); },
                pManager,
                QFileInfo(m_plans.first().nmlPath).absolutePath(),
                this,
                [this]() {
                    QList<NmlImportPlan*> all;
                    for (NmlImportPlan& plan : m_plans) {
                        all.append(&plan);
                    }
                    return all;
                },
                [this]() { return tickedPlans(); });
        connect(pPanels, &NmlReviewPanels::changed, this, &DlgNmlImportPlaylists::refreshRows);
        connect(m_pTable, &QTableWidget::itemChanged, pPanels, &NmlReviewPanels::refresh);
        pLayout->addWidget(pPanels);
    }

    m_pWarning = new QLabel(this);
    pLayout->addWidget(m_pWarning);

    m_pButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_pButtons->button(QDialogButtonBox::Ok)->setText(tr("Import"));
    connect(m_pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    pLayout->addWidget(m_pButtons);

    connect(m_pTable, &QTableWidget::itemChanged, this, &DlgNmlImportPlaylists::updateOkEnabled);
    updateOkEnabled();
    refreshRows();
}

void DlgNmlImportPlaylists::refreshRows() {
    if (!m_pRows) {
        return;
    }
    m_refreshing = true;
    m_pRows->setRowCount(0);
    m_rowRefs.clear();
    for (int i = 0; i < m_plans.size(); ++i) {
        const QTableWidgetItem* pName = m_pTable->item(i, kNameColumn);
        if (!pName || pName->checkState() != Qt::Checked) {
            continue;
        }
        const NmlImportPlan& plan = m_plans.at(i);
        for (int t = 0; t < plan.tracks.size(); ++t) {
            const NmlPlanTrack& track = plan.tracks.at(t);
            if ((track.status == NmlTrackStatus::Found ||
                        track.status == NmlTrackStatus::FoundNextToNml) &&
                    !nmlRowIsReuse(track) && !track.keptSeparate) {
                continue;
            }
            const int row = m_pRows->rowCount();
            m_pRows->insertRow(row);
            m_rowRefs.append({i, t});
            m_pRows->setItem(row, 0, new QTableWidgetItem(pName->text()));
            m_pRows->setItem(row, 1,
                    new QTableWidgetItem(track.title.isEmpty() ? track.primaryKey : track.title));
            m_pRows->setItem(row, 2, new QTableWidgetItem(track.artist));
            m_pRows->setItem(row, 3, newNmlStatusItem(track));
        }
    }
    m_pRows->resizeColumnsToContents();
    m_refreshing = false;
}

void DlgNmlImportPlaylists::onRowChanged(QTableWidgetItem* pItem) {
    if (m_refreshing || pItem->column() != 3 || pItem->row() >= m_rowRefs.size()) {
        return;
    }
    const RowRef ref = m_rowRefs.at(pItem->row());
    const bool ticked = pItem->checkState() == Qt::Checked;
    applyNmlStatusTick(&m_plans[ref.plan], ref.track, ticked);
}

QList<NmlImportPlan*> DlgNmlImportPlaylists::tickedPlans() {
    QList<NmlImportPlan*> ticked;
    for (int i = 0; i < m_plans.size(); ++i) {
        const QTableWidgetItem* pItem = m_pTable->item(i, kNameColumn);
        if (pItem && pItem->checkState() == Qt::Checked) {
            ticked.append(&m_plans[i]);
        }
    }
    return ticked;
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
    // The rows list follows the ticks of the playlists.
    if (m_pRows && !m_refreshing) {
        refreshRows();
    }
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
