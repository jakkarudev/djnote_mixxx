#pragma once

#include <QString>

#include "preferences/usersettings.h"

class QWidget;
class TrackCollectionManager;

namespace mixxx {
namespace nml {

/// DJNote: the interactive flow that imports the playlists of an NML:
/// file dialog (remembered folder) -> Plan -> review dialog -> Apply -> summary.
/// Returns the id of the created Playlist, or kInvalidPlaylistId if the DJ
/// cancelled or nothing was created. Cancel writes nothing.
int runNmlImportFlow(QWidget* pParent,
        TrackCollectionManager* pTrackCollectionManager,
        UserSettingsPointer pConfig);

/// The summary text shown after Apply.
QString nmlImportSummary(const QString& playlistName, int tracksInPlaylist,
        int newLibraryTracks, const QStringList& skipped,
        const QStringList& missingTracks = {},
        const QStringList& matchedLibraryTracks = {}, int droppedCues = 0,
        const QStringList& lockedGridTracks = {},
        const QStringList& reusedTracks = {},
        const QStringList& keptSeparateTracks = {});
} // namespace nml
} // namespace mixxx
