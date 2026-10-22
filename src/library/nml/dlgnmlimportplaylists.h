#pragma once

#include <QDialog>

#include "library/nml/nmlimport.h"

class QDialogButtonBox;
class QLabel;
class QTableWidget;

namespace mixxx {
namespace nml {

/// DJNote: review dialog for an .nml with several playlists (#113). One row per
/// importable playlist: tick box (ticked), editable name, track count.
class DlgNmlImportPlaylists : public QDialog {
    Q_OBJECT
  public:
    DlgNmlImportPlaylists(const QList<NmlPlaylistChoice>& choices,
            QWidget* parent = nullptr);

    /// The choices with the DJ's ticks and names applied.
    QList<NmlPlaylistChoice> choices() const;

  private slots:
    void updateOkEnabled();

  private:
    QList<NmlPlaylistChoice> m_choices;
    QTableWidget* m_pTable;
    QLabel* m_pWarning;
    QDialogButtonBox* m_pButtons;
};

} // namespace nml
} // namespace mixxx
