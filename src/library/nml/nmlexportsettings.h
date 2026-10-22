#pragma once

#include "preferences/configobject.h"

// DJNote: config keys of the NML export (no preferences UI). Kept apart from
// nmlsettings.h so import and export tickets do not touch the same lines.

namespace mixxx {
namespace nml {

/// Base folder for exports: "<folder>/<Playlist name>/<Playlist name>.nml".
inline const ConfigKey kNmlExportLastFolderKey(
        QStringLiteral("[DJNote]"), QStringLiteral("NmlExportLastFolder"));

/// "Copy tracks to the export folder" tick box; default true.
inline const ConfigKey kNmlExportCopyTracksKey(
        QStringLiteral("[DJNote]"), QStringLiteral("NmlExportCopyTracks"));

} // namespace nml
} // namespace mixxx
