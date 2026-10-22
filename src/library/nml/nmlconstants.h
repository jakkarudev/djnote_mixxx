#pragma once

#include <QString>

// DJNote: constants shared by the NML modules and the one analyzer hook.

namespace mixxx {
namespace nml {

/// Beats sub-version of grids imported from a Traktor .nml. The beat analyzer
/// re-analyses such grids only when "Re-analyze beatgrids imported from other
/// DJ software" is on (same treatment as Rekordbox grids).
inline const QString kBeatsSubVersion = QStringLiteral("Traktor NML import");

} // namespace nml
} // namespace mixxx
