#pragma once

#include "preferences/configobject.h"

// DJNote: config keys of the NML exchange feature (no preferences UI).

namespace mixxx {
namespace nml {

/// Folder of the last imported .nml; the import file dialog starts there.
inline const ConfigKey kNmlImportLastFolderKey(
        QStringLiteral("[DJNote]"), QStringLiteral("NmlImportLastFolder"));

} // namespace nml
} // namespace mixxx
