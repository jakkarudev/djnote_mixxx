#include "library/nml/nmlkeepreplacepanel.h"

#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

namespace mixxx {
namespace nml {

NmlKeepReplacePanel::NmlKeepReplacePanel(PlansProvider allPlans,
        TrackCollectionManager* pManager,
        QWidget* parent,
        PlansProvider shownPlans)
        : QWidget(parent),
          m_allPlans(std::move(allPlans)),
          m_shownPlans(shownPlans ? std::move(shownPlans) : m_allPlans),
          m_pManager(pManager) {
    auto* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->addWidget(new QLabel(
            tr("Some tracks are already in your library with preparation:"), this));
    m_pKeep = new QRadioButton(tr("Keep DJNote's preparation"), this);
    m_pKeep->setToolTip(tr("Grid, key, cues, colour and rating are taken from the "
                           "NML only where the track has none of that part."));
    m_pReplace = new QRadioButton(tr("Replace with the NML's (cannot be undone)"), this);
    m_pReplace->setToolTip(tr("Overwrites every part the NML carries, replacing all "
                              "cues. A BPM-locked grid is never replaced."));
    m_pKeep->setChecked(true);
    pLayout->addWidget(m_pKeep);
    pLayout->addWidget(m_pReplace);
    connect(m_pKeep, &QRadioButton::toggled, this, [this]() { applyMode(); });
    refresh();
}

NmlPreparationMode NmlKeepReplacePanel::mode() const {
    return m_pReplace->isChecked() ? NmlPreparationMode::Replace : NmlPreparationMode::Keep;
}

void NmlKeepReplacePanel::applyMode() {
    for (NmlImportPlan* pPlan : m_allPlans()) {
        pPlan->preparationMode = mode();
    }
}

void NmlKeepReplacePanel::refresh() {
    if (m_pManager) {
        for (NmlImportPlan* pPlan : m_allPlans()) {
            updateNmlPlanComparisons(pPlan, m_pManager);
        }
    }
    bool any = false;
    for (const NmlImportPlan* pPlan : m_shownPlans()) {
        any = any || nmlPlanHasExistingPreparation(*pPlan);
    }
    setVisible(any);
    applyMode();
}

} // namespace nml
} // namespace mixxx

#include "moc_nmlkeepreplacepanel.cpp"
