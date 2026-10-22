#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "library/nml/nmldocument.h"
#include "library/nml/nmlkeepreplace.h"
#include "track/trackid.h"

class PlaylistDAO;
class TrackCollectionManager;

// DJNote: GUI-free NML import in two steps.
//   Plan  - read the .nml and resolve every playlist item to a file; writes nothing.
//   Apply - carry out the DJ's choices (name, and in later tickets per-track
//           decisions) against the library.
// The review dialog sits between the two. Later tickets extend NmlPlanTrack
// (e.g. grid/key/cue data comes from `document` via `entryIndex`) and
// NmlImportResult without changing the two function signatures' meaning.

namespace mixxx {
namespace nml {

enum class NmlTrackStatus {
    Found,          // at the recorded path
    FoundNextToNml, // FILE sits next to the .nml (ADR 0009)
    NotFound,
    // DJNote (#111): set by nmlfindmissing.h on rows that were NotFound.
    FoundInBrowsedFolder, // file name found in the folder the DJ browsed to
    MatchedInLibrary,     // proposal: an existing library track (see accepted)
};

struct NmlPlanTrack {
    int entryIndex = -1; // into NmlImportPlan::document.entries, -1 if the playlist item has no entry
    QString primaryKey;
    QString title;
    QString artist;
    QString resolvedPath; // empty when NotFound
    QString recordedPath; // where the NML says the file was (used for missing tracks)
    NmlTrackStatus status = NmlTrackStatus::NotFound;
    // #111: MatchedInLibrary only. The proposed library track and whether the
    // DJ keeps it (tick box). An unticked proposal is applied as missing.
    TrackId libraryTrackId;
    bool accepted = true;
    // #109: set by updateNmlPlanComparisons(). The row lands on a track already
    // in the library (same path, or an accepted library match) and `comparison`
    // is the per-part comparison of that track with the NML entry.
    bool existingTrack = false;
    NmlPreparationComparison comparison;
};

struct NmlImportPlan {
    QString nmlPath;
    QString error; // non-empty: the plan is unusable
    NmlDocument document;
    int playlistIndex = 0; // into document.playlists
    QString defaultName;   // free Playlist name proposed to the DJ
    QList<NmlPlanTrack> tracks; // playlist order, repeats kept
    // #109: what Apply does with rows on existing tracks that have preparation.
    NmlPreparationMode preparationMode = NmlPreparationMode::Keep;
};

struct NmlImportResult {
    QString error;
    int playlistId = -1; // kInvalidPlaylistId when nothing was created
    int tracksInPlaylist = 0;
    int newLibraryTracks = 0;
    QStringList skipped; // one human-readable line per skipped playlist item
    // One line per playlist item that came in as a missing track (no preparation).
    QStringList missingTracks;
    // #108: buttonless cues beyond the main cue and buttonless loops, not imported.
    int droppedCues = 0;
    QStringList matchedLibraryLabels; // #111: one line per accepted library match
    int matchedLibraryTracks = 0; // #111: accepted library matches put into the Playlist
    // #109: rows on existing tracks whose preparation was written (parts taken or replaced).
    int existingTracksUpdated = 0;
    // #109: tracks whose BPM-locked grid kept the NML's different grid out (either mode).
    QStringList lockedGridTracks;
};

/// #109: fills existingTrack/comparison on every row of the plan (looks up the
/// library; writes nothing). Call after planning and after the find-missing
/// operations change rows.
void updateNmlPlanComparisons(NmlImportPlan* pPlan, TrackCollectionManager* pTrackCollectionManager);

/// #109: at least one row lands on an existing track that has preparation, so
/// the review shows the Keep/Replace choice.
bool nmlPlanHasExistingPreparation(const NmlImportPlan& plan);

/// First free Playlist name: `base`, then `base 1`, `base 2` (upstream's
/// import suffix; not PlaylistDAO::createUniquePlaylist which uses "(2)").
QString nextFreePlaylistName(const PlaylistDAO& dao, const QString& base);

NmlImportPlan planNmlImport(const QString& nmlPath,
        const PlaylistDAO& dao,
        int playlistIndex = -1); // -1: the first importable playlist

/// Creates the Playlist `playlistName` and fills it. Not-found items become
/// library tracks flagged missing (titled from the NML, no preparation) and are
/// listed in `missingTracks`; files the library refuses are listed in `skipped`.
NmlImportResult applyNmlImport(const NmlImportPlan& plan,
        const QString& playlistName,
        TrackCollectionManager* pTrackCollectionManager);

// ---- Several playlists in one file (#113) ----

/// Indexes into document.playlists of the playlists worth importing. Skipped:
/// Traktor's special lists (names starting with '_': _LOOPS, _RECORDINGS) and
/// the Preparation list when the file has other lists (a lone playlist named
/// Preparation is the DJ's own export and is kept). Smartlists and Remix Deck
/// sets are never parsed as playlists. Folders are flattened (folderPath is
/// ignored).
QList<int> importableNmlPlaylists(const NmlDocument& document);

/// One row of the multi-playlist review: tick box + editable name.
struct NmlPlaylistChoice {
    int playlistIndex = -1; // into document.playlists
    QString name;           // editable
    int trackCount = 0;
    bool selected = true;
};

/// One choice per importable playlist. A file with exactly one importable
/// playlist keeps the first slice's default (plan.defaultName, from the file
/// name); otherwise each default is the playlist's own name. Defaults get the
/// numeric suffix when taken in the library or by an earlier row.
QList<NmlPlaylistChoice> nmlPlaylistChoices(const NmlImportPlan& plan, const PlaylistDAO& dao);

/// plan.document re-planned for another of its playlists (writes nothing).
NmlImportPlan selectNmlPlaylist(const NmlImportPlan& plan, int playlistIndex);

struct NmlMultiImportResult {
    QString error; // plan unusable
    // One entry per ticked choice, in order. A name taken meanwhile (or by an
    // earlier row) gets the numeric suffix instead of failing.
    QList<NmlImportResult> results;
    QStringList playlistNames; // final names, parallel to results
};

/// #111: one plan per choice (parallel to `choices`), so the find-missing
/// operations can update each playlist's rows before Apply.
QList<NmlImportPlan> nmlPlansForChoices(
        const NmlImportPlan& plan, const QList<NmlPlaylistChoice>& choices);

/// Like the overload below but applies the given (edited) per-choice plans.
NmlMultiImportResult applyNmlImportPlaylists(const QList<NmlImportPlan>& plans,
        const QList<NmlPlaylistChoice>& choices,
        TrackCollectionManager* pTrackCollectionManager);

NmlMultiImportResult applyNmlImportPlaylists(const NmlImportPlan& plan,
        const QList<NmlPlaylistChoice>& choices,
        TrackCollectionManager* pTrackCollectionManager);

} // namespace nml
} // namespace mixxx
