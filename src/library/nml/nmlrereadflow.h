#pragma once

#include "preferences/usersettings.h"

class QWidget;
class TrackCollectionManager;

namespace mixxx {
namespace nml {

/// DJNote: the interactive "Re-read NML" flow for a linked Playlist:
/// browse for the .nml if it moved -> Plan -> review dialog (Replace or Keep)
/// -> confirmation with counts (Replace) -> Apply -> summary.
/// Returns true if the Playlist was re-read.
bool runNmlRereadFlow(QWidget* pParent,
        TrackCollectionManager* pTrackCollectionManager,
        UserSettingsPointer pConfig,
        int playlistId);

} // namespace nml
} // namespace mixxx
