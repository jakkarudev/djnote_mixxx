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
/// name and one row per playlist item with its status. Later tickets add
/// columns/choices here.
class DlgNmlImport : public QDialog {
    Q_OBJECT
  public:
    // `pManager` (optional) enables the "Find in library" button (#111).
    DlgNmlImport(const NmlImportPlan& plan,
            const PlaylistDAO& dao,
            QWidget* parent = nullptr,
            TrackCollectionManager* pManager = nullptr);

    QString playlistName() const;
    /// The plan with the DJ's changes from the "find missing" buttons and tick boxes.
    const NmlImportPlan& plan() const {
        return m_plan;
    }

    static QString statusText(NmlTrackStatus status);

  private slots:
    void updateOkEnabled();
    void browseForMovedFolder(); // #111
    void findInLibrary();        // #111
    void onItemChanged(QTableWidgetItem* pItem);

  private:
    void refreshRows(); // #111: rows from m_plan
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
