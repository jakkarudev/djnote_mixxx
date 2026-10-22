#include "library/nml/nmlimportflow.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>

#include "library/dao/playlistdao.h"
#include "library/nml/dlgnmlimport.h"
#include "library/nml/nmlimport.h"
#include "library/nml/nmlsettings.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"

namespace mixxx {
namespace nml {

namespace {
QString tr(const char* text) {
    return QCoreApplication::translate("NmlImportFlow", text);
}
} // namespace

QString nmlImportSummary(const QString& playlistName,
        int tracksInPlaylist,
        int newLibraryTracks,
        const QStringList& skipped) {
    QString text = tr("Imported %1 tracks into the playlist \"%2\" (%3 new in the library).")
                           .arg(QString::number(tracksInPlaylist),
                                   playlistName,
                                   QString::number(newLibraryTracks));
    if (!skipped.isEmpty()) {
        text += QStringLiteral("\n\n") +
                tr("%1 tracks could not be found and were skipped:")
                        .arg(QString::number(skipped.size())) +
                QStringLiteral("\n") + skipped.join(QLatin1Char('\n'));
    }
    return text;
}

int runNmlImportFlow(QWidget* pParent,
        TrackCollectionManager* pTrackCollectionManager,
        UserSettingsPointer pConfig) {
    const QString startDir = pConfig->getValue(kNmlImportLastFolderKey,
            QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    const QString nmlPath = QFileDialog::getOpenFileName(pParent,
            tr("Import Traktor Playlist"),
            startDir,
            tr("Traktor Collection (*.nml)"));
    if (nmlPath.isEmpty()) {
        return kInvalidPlaylistId;
    }
    pConfig->setValue(kNmlImportLastFolderKey, QFileInfo(nmlPath).absolutePath());

    PlaylistDAO& dao = pTrackCollectionManager->internalCollection()->getPlaylistDAO();
    const NmlImportPlan plan = planNmlImport(nmlPath, dao);
    if (!plan.error.isEmpty()) {
        QMessageBox::warning(pParent,
                tr("Import Traktor Playlist"),
                tr("The file could not be imported: %1").arg(plan.error));
        return kInvalidPlaylistId;
    }

    DlgNmlImport dialog(plan, dao, pParent);
    if (dialog.exec() != QDialog::Accepted) {
        return kInvalidPlaylistId;
    }

    const NmlImportResult result =
            applyNmlImport(plan, dialog.playlistName(), pTrackCollectionManager);
    if (!result.error.isEmpty() && result.playlistId == kInvalidPlaylistId) {
        QMessageBox::warning(pParent, tr("Import Traktor Playlist"), result.error);
        return kInvalidPlaylistId;
    }
    QMessageBox::information(pParent,
            tr("Import Traktor Playlist"),
            nmlImportSummary(dialog.playlistName(),
                    result.tracksInPlaylist,
                    result.newLibraryTracks,
                    result.skipped));
    return result.playlistId;
}

} // namespace nml
} // namespace mixxx
