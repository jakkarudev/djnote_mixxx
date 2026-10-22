#include "library/nml/nmlreviewwidgets.h"

#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "library/nml/nmlfindmissing.h"
#include "library/nml/nmlfindmissingpanel.h"
#include "library/nml/nmlkeepreplacepanel.h"
#include "library/nml/nmlreuse.h"

namespace mixxx {
namespace nml {

QTableWidgetItem* newNmlStatusItem(const NmlPlanTrack& track) {
    auto* pItem = new QTableWidgetItem(nmlRowStatusText(track));
    if (nmlRowIsReuse(track)) {
        pItem->setFlags(pItem->flags() | Qt::ItemIsUserCheckable);
        pItem->setCheckState(track.reuseAccepted ? Qt::Checked : Qt::Unchecked);
    } else if (track.status == NmlTrackStatus::MatchedInLibrary) {
        pItem->setFlags(pItem->flags() | Qt::ItemIsUserCheckable);
        pItem->setCheckState(track.accepted ? Qt::Checked : Qt::Unchecked);
    }
    return pItem;
}

void applyNmlStatusTick(NmlImportPlan* pPlan, int row, bool ticked) {
    setNmlMatchAccepted(pPlan, row, ticked);
    setNmlReuseAccepted(pPlan, row, ticked);
}

NmlReviewPanels::NmlReviewPanels(PlansProvider findPlans,
        TrackCollectionManager* pManager,
        const QString& startFolder,
        QWidget* parent,
        PlansProvider modePlans,
        PlansProvider shownPlans)
        : QWidget(parent) {
    auto* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);

    auto* pFind = new NmlFindMissingPanel(findPlans, pManager, startFolder, this);
    pLayout->addWidget(pFind);
    m_pKeepReplace = new NmlKeepReplacePanel(
            modePlans ? modePlans : findPlans, pManager, this, shownPlans);
    pLayout->addWidget(m_pKeepReplace);

    connect(pFind, &NmlFindMissingPanel::changed, this, [this, findPlans, pManager]() {
        if (pManager) {
            // Rows found by browsing may be copies of library tracks too.
            proposeNmlReuse(findPlans(), pManager);
        }
        m_pKeepReplace->refresh();
        emit changed();
    });
}

void NmlReviewPanels::refresh() {
    m_pKeepReplace->refresh();
}

} // namespace nml
} // namespace mixxx

#include "moc_nmlreviewwidgets.cpp"
