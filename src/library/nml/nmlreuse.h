#pragma once

#include <QList>

#include "library/nml/nmlimport.h"

class TrackCollectionManager;

// DJNote (#112, ADR 0008): "reuse existing track". A found file that is a COPY
// (another path) of a track already in the library is put in the Playlist as
// that library track, instead of being added, when the copy's NML preparation
// is identical to the library track's (kNmlIdenticalTolerance, per part, a
// part present on one side only counts as different). Copies are matched with
// the "Find in library" rules (matchNmlLibraryTrack). A copy whose preparation
// differs, or whose row the DJ unticked, becomes its own library track.

namespace mixxx {
namespace nml {

/// Fills reuseTrackId/reuseAccepted/keptSeparate on the found rows of the plan
/// that are not in the library yet (writes nothing else). Call after planning
/// and after find-missing changes; it resets the fields first.
void proposeNmlReuse(NmlImportPlan* pPlan, TrackCollectionManager* pManager);
void proposeNmlReuse(const QList<NmlImportPlan*>& plans, TrackCollectionManager* pManager);

/// The tick box of a reuse row. Unticked rows are added as their own track.
void setNmlReuseAccepted(NmlImportPlan* pPlan, int row, bool accepted);

/// The row is proposed as reuse of a library track (ticked or not).
inline bool nmlRowIsReuse(const NmlPlanTrack& track) {
    return track.reuseTrackId.isValid();
}

} // namespace nml
} // namespace mixxx
