#pragma once

#include "preferences/usersettings.h"

class QWidget;
class TrackCollectionManager;

namespace mixxx {
namespace nml {

/// DJNote: the interactive flow that makes an NML export of a Playlist:
/// save dialog (default <last folder>/<Playlist name>/<Playlist name>.nml with
/// a remembered "Copy tracks" tick box) -> export worker with progress and
/// cancel -> summary. Returns true if a .nml was written.
bool runNmlExportFlow(QWidget* pParent,
        TrackCollectionManager* pTrackCollectionManager,
        UserSettingsPointer pConfig,
        int playlistId);

} // namespace nml
} // namespace mixxx
