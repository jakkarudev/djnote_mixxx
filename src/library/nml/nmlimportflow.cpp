#include "library/nml/nmlimportflow.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>

#include "library/dao/playlistdao.h"
#include "library/nml/dlgnmlimport.h"
#include "library/nml/dlgnmlimportplaylists.h"
#include "library/nml/nmlimport.h"
#include "library/nml/nmlreuse.h"
#include "library/nml/nmlsettings.h"
#include "library/nml/nmlsupport.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"

namespace mixxx {
namespace nml {

QString nmlImportSummary(const QString& playlistName,
        int tracksInPlaylist,
        int newLibraryTracks,
        const QStringList& skipped,
        const QStringList& missingTracks,
        const QStringList& matchedLibraryTracks,
        int droppedCues,
        const QStringList& lockedGridTracks,
        const QStringList& reusedTracks,
        const QStringList& keptSeparateTracks) {
    QString text = tr("Imported %1 tracks into the playlist \"%2\" (%3 new in the library).")
                           .arg(QString::number(tracksInPlaylist),
                                   playlistName,
                                   QString::number(newLibraryTracks));
    if (!missingTracks.isEmpty()) {
        text += QStringLiteral("\n\n") +
                tr("%1 tracks are missing and came in without preparation "
                   "(re-read the .nml once the files are back):")
                        .arg(QString::number(missingTracks.size())) +
                QStringLiteral("\n") + missingTracks.join(QLatin1Char('\n'));
    }
    if (!matchedLibraryTracks.isEmpty()) {
        text += QStringLiteral("\n\n") +
                tr("%1 tracks were matched to tracks already in the library:")
                        .arg(QString::number(matchedLibraryTracks.size())) +
                QStringLiteral("\n") + matchedLibraryTracks.join(QLatin1Char('\n'));
    }
    if (!reusedTracks.isEmpty()) { // DJNote
        text += QStringLiteral("\n\n") +
                tr("%1 tracks reuse an existing track in the library (identical preparation):")
                        .arg(QString::number(reusedTracks.size())) +
                QStringLiteral("\n") + reusedTracks.join(QLatin1Char('\n'));
    }
    if (!keptSeparateTracks.isEmpty()) { // DJNote
        text += QStringLiteral("\n\n") +
                tr("%1 tracks kept separate: preparation differs:")
                        .arg(QString::number(keptSeparateTracks.size())) +
                QStringLiteral("\n") + keptSeparateTracks.join(QLatin1Char('\n'));
    }
    if (!lockedGridTracks.isEmpty()) { // DJNote
        text += QStringLiteral("\n\n") +
                tr("%1 tracks have a BPM-locked grid; their grid was not replaced:")
                        .arg(QString::number(lockedGridTracks.size())) +
                QStringLiteral("\n") + lockedGridTracks.join(QLatin1Char('\n'));
    }
    if (droppedCues > 0) {
        text += QStringLiteral("\n\n") +
                tr("%1 cues were not imported (cues without a hotcue button beyond "
                   "the main cue, and loops without a button).")
                        .arg(QString::number(droppedCues));
    }
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
            tr("Import NML"),
            startDir,
            tr("Traktor Collection (*.nml)"));
    if (nmlPath.isEmpty()) {
        return kInvalidPlaylistId;
    }
    pConfig->setValue(kNmlImportLastFolderKey, QFileInfo(nmlPath).absolutePath());

    PlaylistDAO& dao = pTrackCollectionManager->internalCollection()->getPlaylistDAO();
    NmlImportPlan plan = planNmlImport(nmlPath, dao);
    if (!plan.error.isEmpty()) {
        QMessageBox::warning(pParent,
                tr("Import NML"),
                tr("The file could not be imported: %1").arg(plan.error));
        return kInvalidPlaylistId;
    }

    updateNmlPlanComparisons(&plan, pTrackCollectionManager);
    proposeNmlReuse(&plan, pTrackCollectionManager);
    // a file with several playlists gets the tick-box review instead.
    const QList<NmlPlaylistChoice> choices = nmlPlaylistChoices(plan, dao);
    if (choices.size() > 1) {
        DlgNmlImportPlaylists multiDialog(choices, plan, pTrackCollectionManager, pParent);
        if (multiDialog.exec() != QDialog::Accepted) {
            return kInvalidPlaylistId;
        }
        const NmlMultiImportResult multi = applyNmlImportPlaylists(
                multiDialog.plans(), multiDialog.choices(), pTrackCollectionManager);
        QStringList lines;
        int lastPlaylistId = kInvalidPlaylistId;
        for (int i = 0; i < multi.results.size(); ++i) {
            const NmlImportResult& r = multi.results.at(i);
            if (!r.error.isEmpty() && r.playlistId == kInvalidPlaylistId) {
                lines.append(multi.playlistNames.at(i) + QStringLiteral(": ") + r.error);
                continue;
            }
            lines.append(nmlImportSummary(multi.playlistNames.at(i),
                    r.tracksInPlaylist,
                    r.newLibraryTracks,
                    r.skipped,
                    r.missingTracks,
                    r.matchedLibraryLabels,
                    r.droppedCues,
                    r.lockedGridTracks,
                    r.reusedTrackLabels,
                    r.keptSeparateLabels));
            lastPlaylistId = r.playlistId;
        }
        QMessageBox::information(pParent,
                tr("Import NML"),
                lines.join(QStringLiteral("\n\n")));
        return lastPlaylistId;
    }

    DlgNmlImport dialog(plan, dao, pParent, pTrackCollectionManager);
    if (dialog.exec() != QDialog::Accepted) {
        return kInvalidPlaylistId;
    }

    const NmlImportResult result =
            applyNmlImport(dialog.plan(), dialog.playlistName(), pTrackCollectionManager);
    if (!result.error.isEmpty() && result.playlistId == kInvalidPlaylistId) {
        QMessageBox::warning(pParent, tr("Import NML"), result.error);
        return kInvalidPlaylistId;
    }
    QMessageBox::information(pParent,
            tr("Import NML"),
            nmlImportSummary(dialog.playlistName(),
                    result.tracksInPlaylist,
                    result.newLibraryTracks,
                    result.skipped,
                    result.missingTracks,
                    result.matchedLibraryLabels,
                    result.droppedCues,
                    result.lockedGridTracks,
                    result.reusedTrackLabels,
                    result.keptSeparateLabels));
    return result.playlistId;
}

} // namespace nml
} // namespace mixxx
