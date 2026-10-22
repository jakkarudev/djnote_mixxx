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
//   Apply - carry out the DJ's choices (name, per-track decisions) against the
//           library.
// The review dialog sits between the two. Preparation data comes from
// `document` via NmlPlanTrack::entryIndex.

namespace mixxx {
namespace nml {

enum class NmlTrackStatus {
    Found,          // at the recorded path
    FoundNextToNml, // FILE sits next to the .nml (ADR 0009)
    NotFound,
    // DJNote: set by nmlfindmissing.h on rows that were NotFound.
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
    // MatchedInLibrary only: the proposed library track and whether the
    // DJ keeps it (tick box). An unticked proposal is applied as missing.
    TrackId libraryTrackId;
    bool accepted = true;
    // Set by updateNmlPlanComparisons(). The row lands on a track already
    // in the library (same path, or an accepted library match) and `comparison`
    // is the per-part comparison of that track with the NML entry.
    bool existingTrack = false;
    NmlPreparationComparison comparison;
    // (nmlreuse.h) A found file that is a copy of a library track with
    // identical preparation. reuseTrackId is that track; the tick box is
    // reuseAccepted. keptSeparate: a copy of a library track whose
    // preparation differs (listed in the summary).
    TrackId reuseTrackId;
    bool reuseAccepted = true;
    bool keptSeparate = false;
};

struct NmlImportPlan {
    QString nmlPath;
    QString error; // non-empty: the plan is unusable
    NmlDocument document;
    int playlistIndex = 0; // into document.playlists
    QString defaultName;   // free Playlist name proposed to the DJ
    QList<NmlPlanTrack> tracks; // playlist order, repeats kept
    // What Apply does with rows on existing tracks that have preparation.
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
    // Buttonless cues beyond the main cue and buttonless loops, not imported.
    int droppedCues = 0;
    QStringList matchedLibraryLabels; // one line per accepted library match
    int matchedLibraryTracks = 0; // accepted library matches put into the Playlist
    // Rows on existing tracks whose preparation was written (parts taken or replaced).
    int existingTracksUpdated = 0;
    // Tracks whose BPM-locked grid kept the NML's different grid out (either mode).
    QStringList lockedGridTracks;
    // Copies that came in as the library track they duplicate.
    QStringList reusedTrackLabels;
    // Copies of a library track added as their own track (preparation
    // differs, or the DJ unticked the reuse row).
    QStringList keptSeparateLabels;
};

/// How the review dialogs word a row's status ("found", "missing", ...).
QString nmlStatusText(NmlTrackStatus status);

/// The status plus the reuse wording: "reuse existing track", "kept separate: ...".
QString nmlRowStatusText(const NmlPlanTrack& track);

/// Fills existingTrack/comparison on every row of the plan (looks up the
/// library; writes nothing). Call after planning and after the find-missing
/// operations change rows.
void updateNmlPlanComparisons(NmlImportPlan* pPlan, TrackCollectionManager* pTrackCollectionManager);

/// True when at least one row lands on an existing track that has preparation, so
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

// ---- Several playlists in one file ----

/// Indexes into document.playlists of the playlists worth importing. Skipped:
/// Traktor's special lists (names starting with '_': _LOOPS, _RECORDINGS).
/// A list named Preparation comes last: a file with other lists offers it
/// UNTICKED (see nmlPlaylistChoices); alone it is the DJ's own export and is
/// imported as usual. Smartlists and Remix Deck sets are never parsed as
/// playlists. Folders are flattened (folderPath is ignored).
QList<int> importableNmlPlaylists(const NmlDocument& document);

/// One row of the multi-playlist review: tick box + editable name.
struct NmlPlaylistChoice {
    int playlistIndex = -1; // into document.playlists
    QString name;           // editable
    int trackCount = 0;
    bool selected = true;
};

/// One choice per importable playlist. A file with exactly one importable
/// playlist keeps the file-name default (plan.defaultName, from the file
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

/// One plan per choice (parallel to `choices`), so the find-missing
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
