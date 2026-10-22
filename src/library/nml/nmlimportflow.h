#pragma once

#include <QString>

#include "preferences/usersettings.h"

class QWidget;
class TrackCollectionManager;

namespace mixxx {
namespace nml {

/// DJNote: the interactive import flow behind "Import Traktor Playlist (.nml)...":
/// file dialog (remembered folder) -> Plan -> review dialog -> Apply -> summary.
/// Returns the id of the created Playlist, or kInvalidPlaylistId if the DJ
/// cancelled or nothing was created. Cancel writes nothing.
int runNmlImportFlow(QWidget* pParent,
        TrackCollectionManager* pTrackCollectionManager,
        UserSettingsPointer pConfig);

/// The summary text shown after Apply.
QString nmlImportSummary(const QString& playlistName, int tracksInPlaylist,
        int newLibraryTracks, const QStringList& skipped,
        const QStringList& missingTracks = {}, int droppedCues = 0);

} // namespace nml
} // namespace mixxx
