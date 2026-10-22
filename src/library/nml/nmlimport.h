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
        int playlistIndex = 0);

/// Creates the Playlist `playlistName` and fills it. Not-found items (and
/// files the library refuses) are skipped and listed in `skipped`.
NmlImportResult applyNmlImport(const NmlImportPlan& plan,
        const QString& playlistName,
        TrackCollectionManager* pTrackCollectionManager);

} // namespace nml
} // namespace mixxx
