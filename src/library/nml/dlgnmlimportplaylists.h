#pragma once

#include <QDialog>

#include "library/nml/nmlimport.h"

class QDialogButtonBox;
class QLabel;
class QTableWidget;
class QTableWidgetItem;
class TrackCollectionManager;

namespace mixxx {
namespace nml {

/// DJNote: review dialog for an .nml with several playlists. One row per
/// importable playlist: tick box (ticked), editable name, track count.
class DlgNmlImportPlaylists : public QDialog {
    Q_OBJECT
  public:
    DlgNmlImportPlaylists(const QList<NmlPlaylistChoice>& choices,
            QWidget* parent = nullptr);

    /// with the plan of the file the dialog also offers "Browse for the
    /// moved folder..." / "Find in library" for the ticked playlists' missing
    /// rows (pManager null: no library button). Read the result with plans().
    DlgNmlImportPlaylists(const QList<NmlPlaylistChoice>& choices,
            const NmlImportPlan& plan,
            TrackCollectionManager* pManager,
            QWidget* parent = nullptr);

    /// One plan per choice (parallel to choices()), with the DJ's find-missing
    /// changes and tick boxes; pass to applyNmlImportPlaylists.
    const QList<NmlImportPlan>& plans() const {
        return m_plans;
    }

    /// The choices with the DJ's ticks and names applied.
    QList<NmlPlaylistChoice> choices() const;

  private slots:
    void updateOkEnabled();
    void refreshRows();
    void onRowChanged(QTableWidgetItem* pItem);

  private:
    QList<NmlImportPlan*> tickedPlans();
    QList<NmlPlaylistChoice> m_choices;
    void init(TrackCollectionManager* pManager);

    struct RowRef {
        int plan;
        int track;
    };
    QList<RowRef> m_rowRefs; // m_pRows row -> plan/track
    QList<NmlImportPlan> m_plans;
    QTableWidget* m_pRows = nullptr; // missing / matched rows of ticked playlists
    bool m_refreshing = false;
    QTableWidget* m_pTable;
    QLabel* m_pWarning;
    QDialogButtonBox* m_pButtons;
};

} // namespace nml
} // namespace mixxx
