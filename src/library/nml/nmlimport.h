#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "library/nml/nmldocument.h"

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
};

struct NmlPlanTrack {
    int entryIndex = -1; // into NmlImportPlan::document.entries, -1 if the playlist item has no entry
    QString primaryKey;
    QString title;
    QString artist;
    QString resolvedPath; // empty when NotFound
    NmlTrackStatus status = NmlTrackStatus::NotFound;
};

struct NmlImportPlan {
    QString nmlPath;
    QString error; // non-empty: the plan is unusable
    NmlDocument document;
    int playlistIndex = 0; // into document.playlists
    QString defaultName;   // free Playlist name proposed to the DJ
    QList<NmlPlanTrack> tracks; // playlist order, repeats kept
};

struct NmlImportResult {
    QString error;
    int playlistId = -1; // kInvalidPlaylistId when nothing was created
    int tracksInPlaylist = 0;
    int newLibraryTracks = 0;
    QStringList skipped; // one human-readable line per skipped playlist item
};

/// First free Playlist name: `base`, then `base 1`, `base 2` (upstream's
/// import suffix; not PlaylistDAO::createUniquePlaylist which uses "(2)").
QString nextFreePlaylistName(const PlaylistDAO& dao, const QString& base);

NmlImportPlan planNmlImport(const QString& nmlPath,
        const PlaylistDAO& dao,
        int playlistIndex = -1); // -1: the first importable playlist

/// Creates the Playlist `playlistName` and fills it. Not-found items (and
/// files the library refuses) are skipped and listed in `skipped`.
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

NmlMultiImportResult applyNmlImportPlaylists(const NmlImportPlan& plan,
        const QList<NmlPlaylistChoice>& choices,
        TrackCollectionManager* pTrackCollectionManager);

} // namespace nml
} // namespace mixxx
