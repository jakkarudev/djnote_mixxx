#pragma once

#include <QList>
#include <QSet>
#include <QString>

#include "library/nml/nmlimport.h"

class TrackCollectionManager;

// DJNote (#111): GUI-free operations on the import plan that find tracks the
// plan marked NotFound. The review dialog only calls these and re-reads the
// rows. Both act on NotFound rows only and update them in place.

namespace mixxx {
namespace nml {

/// Largest duration difference for a library match (seconds, inclusive).
constexpr double kNmlLibraryMatchToleranceSeconds = 2.0;

/// Browse for the moved folder: every NotFound row is looked for by file name
/// (flat, as the export folder layout is) in `folder`. Rows found become
/// FoundInBrowsedFolder with resolvedPath set. Returns how many were found.
int browseNmlMissingInFolder(NmlImportPlan* pPlan, const QString& folder);

/// A library track as far as the matching rules need it.
struct NmlLibraryCandidate {
    TrackId id;
    QString fileName; // without directory
    QString artist;
    QString title;
    double durationSeconds = 0.0;
};

/// Library tracks that are present on disk and not removed (missing and hidden
/// tracks are never proposed).
QList<NmlLibraryCandidate> nmlLibraryCandidates(TrackCollectionManager* pManager);

/// THE library-matching rules (reused by "reuse identical copies"). For an NML
/// item (file name, artist, title, playtime) returns the id of the best
/// candidate or an invalid id:
///  1. same file name (case-insensitive) and duration within
///     kNmlLibraryMatchToleranceSeconds;
///  2. else same artist and title (case-insensitive, trimmed, both non-empty)
///     and duration within the tolerance.
/// An unknown playtime (<= 0) never matches. Ties go to the closest duration.
/// Ids in `exclude` are skipped.
TrackId matchNmlLibraryTrack(const QList<NmlLibraryCandidate>& candidates,
        const QString& fileName,
        const QString& artist,
        const QString& title,
        double playtimeSeconds,
        const QSet<TrackId>& exclude = {});

/// "Find in library" on already loaded candidates. Matched rows become
/// MatchedInLibrary with libraryTrackId set and accepted = true. Returns how
/// many rows were proposed.
int findNmlMissingInLibrary(NmlImportPlan* pPlan,
        const QList<NmlLibraryCandidate>& candidates);

/// Convenience: loads the candidates from the library first.
int findNmlMissingInLibrary(NmlImportPlan* pPlan, TrackCollectionManager* pManager);

/// The tick box of a MatchedInLibrary row. Unticked rows stay missing on Apply.
void setNmlMatchAccepted(NmlImportPlan* pPlan, int row, bool accepted);

// ---- Several playlists in one file: the same operations over all plans ----
// `plans` are the per-playlist plans of the ticked playlists (nmlPlansForChoices).

int browseNmlMissingInFolder(const QList<NmlImportPlan*>& plans, const QString& folder);
int findNmlMissingInLibrary(const QList<NmlImportPlan*>& plans, TrackCollectionManager* pManager);

} // namespace nml
} // namespace mixxx
