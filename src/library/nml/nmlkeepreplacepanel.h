#pragma once

#include <QWidget>

#include <functional>

#include "library/nml/nmlimport.h"

class QRadioButton;
class TrackCollectionManager;

namespace mixxx {
namespace nml {

/// DJNote (#109): the "Keep DJNote's preparation / Replace with the NML's"
/// choice shared by both import review dialogs. Hidden unless at least one row
/// of the plans lands on an existing track that has preparation. It sets
/// NmlImportPlan::preparationMode on every plan of `allPlans`.
class NmlKeepReplacePanel : public QWidget {
    Q_OBJECT
  public:
    using PlansProvider = std::function<QList<NmlImportPlan*>()>;

    /// `allPlans`: every plan the mode applies to. `shownPlans` (optional,
    /// default allPlans): the plans that decide whether the choice is shown
    /// (e.g. the ticked playlists).
    NmlKeepReplacePanel(PlansProvider allPlans,
            TrackCollectionManager* pManager,
            QWidget* parent = nullptr,
            PlansProvider shownPlans = nullptr);

    NmlPreparationMode mode() const;

  public slots:
    /// Recomputes the per-part comparisons (rows may have changed) and shows or
    /// hides the choice.
    void refresh();

  private:
    void applyMode();

    PlansProvider m_allPlans;
    PlansProvider m_shownPlans;
    TrackCollectionManager* m_pManager;
    QRadioButton* m_pKeep;
    QRadioButton* m_pReplace;
};

} // namespace nml
} // namespace mixxx
