#pragma once

#include <QDialog>

#include "library/nml/nmlreread.h"

class QRadioButton;
class QTableWidget;
class TrackCollectionManager;

namespace mixxx {
namespace nml {

/// DJNote (#114): review dialog of "Re-read NML". Asks Replace or Keep, shows
/// every item of the NML playlist with its status, and offers the same
/// find-missing and keep/replace-preparation panels as the import.
class DlgNmlReread : public QDialog {
    Q_OBJECT
  public:
    DlgNmlReread(const NmlRereadPlan& plan,
            const QString& playlistName,
            TrackCollectionManager* pManager,
            QWidget* parent = nullptr);

    NmlRereadMode mode() const;
    /// The plan with the DJ's changes from the find-missing buttons.
    const NmlRereadPlan& plan() const {
        return m_plan;
    }

  private slots:
    void refreshRows();

  private:
    NmlRereadPlan m_plan;
    TrackCollectionManager* m_pManager;
    QTableWidget* m_pTable;
    QRadioButton* m_pReplace;
    QRadioButton* m_pKeep;
};

} // namespace nml
} // namespace mixxx
