#pragma once

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QString>

#include "library/nml/nmlimport.h"
#include "track/trackid.h"

class TrackCollectionManager;

// DJNote: small helpers shared by the NML exchange modules.

namespace mixxx {
namespace nml {

/// Translation of a string of the NML exchange (one context for all of it).
inline QString tr(const char* text) {
    return QCoreApplication::translate("NmlExchange", text);
}

/// The library database every NML module reads and writes.
QSqlDatabase nmlDatabase(TrackCollectionManager* pManager);

/// "Artist - Title" of a plan row, falling back to the title or the playlist key.
QString nmlPlanTrackLabel(const NmlPlanTrack& item);

/// Gives the not-found row `item` a library row at its recorded path, flagged
/// missing (track_locations.fs_deleted) and titled from the NML for a new row;
/// returns false when that is not possible. A file that is not there can't be
/// read, so nothing of its preparation is written.
bool ensureNmlMissingTrack(
        TrackCollectionManager* pManager, const NmlPlanTrack& item, TrackId* pId);

} // namespace nml
} // namespace mixxx
