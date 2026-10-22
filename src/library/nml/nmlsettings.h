#pragma once

#include "preferences/configobject.h"

// DJNote: config keys of the NML exchange feature (no preferences UI).

namespace mixxx {
namespace nml {

/// Folder of the last imported .nml; the import file dialog starts there.
inline const ConfigKey kNmlImportLastFolderKey(
        QStringLiteral("[DJNote]"), QStringLiteral("NmlImportLastFolder"));

/// Base folder for exports: "<folder>/<Playlist name>/<Playlist name>.nml".
inline const ConfigKey kNmlExportLastFolderKey(
        QStringLiteral("[DJNote]"), QStringLiteral("NmlExportLastFolder"));

/// "Copy tracks to the export folder" tick box; default true.
inline const ConfigKey kNmlExportCopyTracksKey(
        QStringLiteral("[DJNote]"), QStringLiteral("NmlExportCopyTracks"));

} // namespace nml
} // namespace mixxx
