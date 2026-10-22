#pragma once

#include <QDialog>

#include "library/nml/nmlimport.h"

class PlaylistDAO;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;
class TrackCollectionManager;

namespace mixxx {
namespace nml {

/// DJNote: review dialog between Plan and Apply. Shows the editable Playlist
/// name and one row per playlist item with its status.
class DlgNmlImport : public QDialog {
    Q_OBJECT
  public:
    // `pManager` (optional) enables the "Find in library" button.
    DlgNmlImport(const NmlImportPlan& plan,
            const PlaylistDAO& dao,
            QWidget* parent = nullptr,
            TrackCollectionManager* pManager = nullptr);

    QString playlistName() const;
    /// The plan with the DJ's changes from the "find missing" buttons and tick boxes.
    const NmlImportPlan& plan() const {
        return m_plan;
    }

  private slots:
    void updateOkEnabled();
    void refreshRows(); // rows from m_plan
    void onItemChanged(QTableWidgetItem* pItem);

  private:
    NmlImportPlan m_plan;
    const PlaylistDAO& m_dao;
    TrackCollectionManager* m_pManager;
    QTableWidget* m_pTable;
    bool m_refreshing = false;
    QLineEdit* m_pNameEdit;
    QLabel* m_pNameWarning;
    QDialogButtonBox* m_pButtons;
};

} // namespace nml
} // namespace mixxx
