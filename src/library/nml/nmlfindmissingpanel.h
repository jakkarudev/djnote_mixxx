#pragma once

#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

#include "library/nml/nmlimport.h"

class TrackCollectionManager;

namespace mixxx {
namespace nml {

/// DJNote: the "Browse for the moved folder..." and "Find in library"
/// buttons shared by both import review dialogs. They only call the GUI-free
/// plan operations of nmlfindmissing.h on the plans `plansProvider` returns
/// (one plan for a single playlist, the ticked playlists' plans otherwise) and
/// emit `changed()` so the dialog can redraw its rows.
class NmlFindMissingPanel : public QWidget {
    Q_OBJECT
  public:
    using PlansProvider = std::function<QList<NmlImportPlan*>()>;

    NmlFindMissingPanel(PlansProvider plansProvider,
            TrackCollectionManager* pManager, // null hides "Find in library"
            const QString& startFolder,
            QWidget* parent = nullptr);

  signals:
    void changed();

  private:
    void browse();
    void findInLibrary();

    PlansProvider m_plansProvider;
    TrackCollectionManager* m_pManager;
    QString m_startFolder;
};

} // namespace nml
} // namespace mixxx
