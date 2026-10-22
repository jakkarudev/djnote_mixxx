#pragma once

#include <QDialog>

#include "library/nml/nmlimport.h"

class PlaylistDAO;
class QDialogButtonBox;
class QLabel;
class QLineEdit;

namespace mixxx {
namespace nml {

/// DJNote: review dialog between Plan and Apply. Shows the editable Playlist
/// name and one row per playlist item with its status. Later tickets add
/// columns/choices here.
class DlgNmlImport : public QDialog {
    Q_OBJECT
  public:
    DlgNmlImport(const NmlImportPlan& plan, const PlaylistDAO& dao, QWidget* parent = nullptr);

    QString playlistName() const;

    static QString statusText(NmlTrackStatus status);

  private slots:
    void updateOkEnabled();

  private:
    const PlaylistDAO& m_dao;
    QLineEdit* m_pNameEdit;
    QLabel* m_pNameWarning;
    QDialogButtonBox* m_pButtons;
};

} // namespace nml
} // namespace mixxx
