#include "library/nml/nmlrereadflow.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include "library/dao/playlistdao.h"
#include "library/nml/dlgnmlreread.h"
#include "library/nml/nmlreread.h"
#include "library/nml/nmlsettings.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"

namespace mixxx {
namespace nml {

namespace {
QString tr(const char* text) {
    return QCoreApplication::translate("NmlRereadFlow", text);
}
} // namespace

bool runNmlRereadFlow(QWidget* pParent,
        TrackCollectionManager* pTrackCollectionManager,
        UserSettingsPointer pConfig,
        int playlistId) {
    NmlRereadPlan plan = planNmlReread(playlistId, pTrackCollectionManager);
    if (plan.nmlFileMissing) {
        // The .nml moved: ask where it is now; Apply remembers the new path.
        const QString browsed = QFileDialog::getOpenFileName(pParent,
                tr("Where is the .nml now?"),
                QFileInfo(plan.link.nmlPath).absolutePath(),
                tr("Traktor Collection (*.nml)"));
        if (browsed.isEmpty()) {
            return false;
        }
        pConfig->setValue(kNmlImportLastFolderKey, QFileInfo(browsed).absolutePath());
        plan = planNmlReread(playlistId, pTrackCollectionManager, browsed);
    }
    if (!plan.error.isEmpty()) {
        QMessageBox::warning(pParent, tr("Re-read NML"), plan.error);
        return false;
    }

    PlaylistDAO& dao = pTrackCollectionManager->internalCollection()->getPlaylistDAO();
    const QString playlistName = dao.getPlaylistName(playlistId);
    DlgNmlReread dialog(plan, playlistName, pTrackCollectionManager, pParent);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    const NmlRereadMode mode = dialog.mode();
    const NmlRereadPlan& reviewed = dialog.plan();
    if (mode == NmlRereadMode::Replace) {
        const NmlRereadCounts counts = nmlRereadCounts(reviewed, pTrackCollectionManager);
        const auto answer = QMessageBox::question(pParent,
                tr("Re-read NML"),
                tr("Replace the list of \"%1\" with the .nml's current list?\n\n"
                   "%2 tracks will be added and %3 removed.")
                        .arg(playlistName,
                                QString::number(counts.added),
                                QString::number(counts.removed)),
                QMessageBox::Ok | QMessageBox::Cancel);
        if (answer != QMessageBox::Ok) {
            return false;
        }
    }
    const NmlRereadResult result = applyNmlReread(reviewed, mode, pTrackCollectionManager);
    if (!result.error.isEmpty()) {
        QMessageBox::warning(pParent, tr("Re-read NML"), result.error);
        return false;
    }
    QMessageBox::information(
            pParent, tr("Re-read NML"), nmlRereadSummary(playlistName, mode, result));
    return true;
}

} // namespace nml
} // namespace mixxx
