#pragma once

#include <QWidget>
#include <functional>

#include "library/nml/nmlimport.h"

class QTableWidgetItem;
class TrackCollectionManager;

namespace mixxx {
namespace nml {
class NmlKeepReplacePanel;
} // namespace nml
} // namespace mixxx

// DJNote: the pieces every NML review dialog (import, import of several
// playlists, re-read) is built from, so the dialogs do not repeat them.

namespace mixxx {
namespace nml {

/// The Status cell of a plan row: the row's status text, with a tick box for
/// rows the DJ can accept or refuse (a library match or a reuse of an existing
/// track).
QTableWidgetItem* newNmlStatusItem(const NmlPlanTrack& track);

/// Applies the state of a Status cell's tick box to row `row` of the plan.
void applyNmlStatusTick(NmlImportPlan* pPlan, int row, bool ticked);

/// The "Browse for the moved folder..." / "Find in library" buttons and the
/// keep/replace choice under a review table. Changes to the rows (found files,
/// library matches) re-propose the reuse of existing tracks first, then
/// `changed()` tells the dialog to redraw its table.
class NmlReviewPanels : public QWidget {
    Q_OBJECT
  public:
    using PlansProvider = std::function<QList<NmlImportPlan*>()>;

    /// `findPlans`: the plans the buttons act on. `modePlans` (default
    /// findPlans): every plan the keep/replace choice applies to. `shownPlans`
    /// (default modePlans): the plans that decide whether the choice is shown.
    /// `pManager` null: no library button, no reuse proposals.
    NmlReviewPanels(PlansProvider findPlans,
            TrackCollectionManager* pManager,
            const QString& startFolder,
            QWidget* parent,
            PlansProvider modePlans = nullptr,
            PlansProvider shownPlans = nullptr);

  signals:
    void changed();

  public slots:
    /// Recomputes the keep/replace choice (rows or ticked playlists changed).
    void refresh();

  private:
    NmlKeepReplacePanel* m_pKeepReplace;
};

} // namespace nml
} // namespace mixxx
