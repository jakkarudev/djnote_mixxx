#pragma once

#include <QString>
#include <QStringList>

#include "library/nml/nmlimport.h"
#include "library/nml/nmlsourcelink.h"

class TrackCollectionManager;

// DJNote (#114): "Re-read NML" = the existing Plan + Apply run against the
// Playlist that remembers its source NML (nmlsourcelink.h), GUI-free.
//   Replace: the Playlist's tracks and order become the NML's current list.
//   Keep:    the list is unchanged; missing tracks whose file is now found are
//            relinked and get the preparation they lack.
// In both modes a missing track whose file is found is relinked (its library
// row's location is updated, same library id, no second track).

namespace mixxx {
namespace nml {

enum class NmlRereadMode {
    Replace,
    Keep,
};

struct NmlRereadPlan {
    int playlistId = -1;
    NmlSourceLink link;
    QString error;            // non-empty: unusable (see the flags for the reason)
    bool locked = false;      // the Playlist is locked: re-read refused
    bool nmlFileMissing = false; // the .nml is not at link.nmlPath: ask the DJ to browse
    NmlImportPlan plan;       // the playlist inside the .nml, resolved to files
};

/// Reads the source NML again and plans against the linked Playlist. Writes
/// nothing. `nmlPathOverride` (the DJ browsed for a moved .nml) replaces the
/// remembered path; Apply then stores it. The NML playlist is found by UUID,
/// falling back to its name.
NmlRereadPlan planNmlReread(int playlistId,
        TrackCollectionManager* pTrackCollectionManager,
        const QString& nmlPathOverride = QString());

/// Replace mode: how many tracks the re-read adds to and removes from the Playlist.
struct NmlRereadCounts {
    int added = 0;
    int removed = 0;
};
NmlRereadCounts nmlRereadCounts(
        const NmlRereadPlan& plan, TrackCollectionManager* pTrackCollectionManager);

struct NmlRereadResult {
    QString error;
    int tracksInPlaylist = 0;
    int added = 0;   // Replace: as nmlRereadCounts
    int removed = 0; // Replace: as nmlRereadCounts
    QStringList relinked;       // missing tracks whose file was found (labels)
    int newLibraryTracks = 0;   // Replace: tracks new to the library
    QStringList missingTracks;  // items still missing after the re-read
    int droppedCues = 0;
    int existingTracksUpdated = 0;
    QStringList lockedGridTracks;
};

/// Applies the re-read. Refused (error) on a locked Playlist. Updates the
/// link (path, UUID, name) from the NML.
NmlRereadResult applyNmlReread(const NmlRereadPlan& plan,
        NmlRereadMode mode,
        TrackCollectionManager* pTrackCollectionManager);

QString nmlRereadSummary(const QString& playlistName, NmlRereadMode mode, const NmlRereadResult& r);

} // namespace nml
} // namespace mixxx
