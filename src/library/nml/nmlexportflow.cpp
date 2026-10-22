#include "library/nml/nmlexportflow.h"

#include <QCheckBox>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QMessageBox>
#include <QProgressDialog>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlexport.h"
#include "library/nml/nmlsettings.h"
#include "library/nml/nmlsupport.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"

namespace mixxx {
namespace nml {

bool runNmlExportFlow(QWidget* pParent,
        TrackCollectionManager* pTrackCollectionManager,
        UserSettingsPointer pConfig,
        int playlistId) {
    PlaylistDAO& dao = pTrackCollectionManager->internalCollection()->getPlaylistDAO();
    const QString playlistName = dao.getPlaylistName(playlistId);

    const QString defaultPath = nmlDefaultExportPath(
            pConfig->getValue(kNmlExportLastFolderKey, QString()), playlistName);
    QDir().mkpath(QFileInfo(defaultPath).absolutePath());

    // A non-native save dialog so the tick box can sit inside it.
    QFileDialog dialog(pParent,
            tr("Export NML"),
            defaultPath,
            tr("Traktor Collection (*.nml)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(QStringLiteral("nml"));
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    QCheckBox copyBox(tr("Copy tracks to the export folder"), &dialog);
    copyBox.setChecked(pConfig->getValue(kNmlExportCopyTracksKey, true));
    if (auto* pLayout = qobject_cast<QGridLayout*>(dialog.layout())) {
        pLayout->addWidget(&copyBox, pLayout->rowCount(), 0, 1, -1);
    }
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return false;
    }
    NmlExportJob job;
    job.nmlPath = dialog.selectedFiles().constFirst();
    job.playlistName = playlistName;
    job.copyTracks = copyBox.isChecked();
    pConfig->setValue(kNmlExportCopyTracksKey, job.copyTracks);
    pConfig->setValue(kNmlExportLastFolderKey, nmlFolderToRemember(job.nmlPath, playlistName));
    job.items = nmlExportItemsForPlaylist(pTrackCollectionManager, playlistId);

    NmlExportWorker worker(job);
    QProgressDialog progress(tr("Exporting..."), tr("Cancel"), 0, job.items.size(), pParent);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    QObject::connect(&worker,
            &NmlExportWorker::progress,
            &progress,
            [&progress](const QString& name, int index, int) {
                progress.setLabelText(name);
                progress.setValue(index);
            });
    QObject::connect(&progress,
            &QProgressDialog::canceled,
            &worker,
            &NmlExportWorker::stop,
            Qt::DirectConnection);
    QEventLoop loop;
    QObject::connect(&worker, &QThread::finished, &loop, &QEventLoop::quit);
    worker.start();
    loop.exec();
    progress.close();

    const NmlExportResult& result = worker.result();
    const QString title = tr("Export NML");
    if (!result.error.isEmpty()) {
        QMessageBox::warning(pParent, title, nmlExportSummary(result));
        return false;
    }
    QMessageBox::information(pParent, title, nmlExportSummary(result));
    return !result.canceled;
}

} // namespace nml
} // namespace mixxx
