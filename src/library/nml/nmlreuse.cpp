#include "library/nml/nmlreuse.h"

#include <QFileInfo>

#include "library/nml/nmlfindmissing.h"
#include "library/nml/nmlkeepreplace.h"
#include "library/trackcollectionmanager.h"
#include "track/track.h"
#include "track/trackref.h"

namespace mixxx {
namespace nml {

namespace {
bool isFoundFile(NmlTrackStatus status) {
    return status == NmlTrackStatus::Found || status == NmlTrackStatus::FoundNextToNml ||
            status == NmlTrackStatus::FoundInBrowsedFolder;
}

void proposeWithCandidates(NmlImportPlan* pPlan,
        const QList<NmlLibraryCandidate>& candidates,
        TrackCollectionManager* pManager) {
    for (NmlPlanTrack& item : pPlan->tracks) {
        item.reuseTrackId = TrackId();
        item.reuseAccepted = true;
        item.keptSeparate = false;
        if (!isFoundFile(item.status) || item.entryIndex < 0 || item.resolvedPath.isEmpty()) {
            continue;
        }
        // Only copies: the file itself is not in the library yet.
        if (pManager->getTrackByRef(TrackRef::fromFilePath(item.resolvedPath))) {
            continue;
        }
        const NmlEntry& entry = pPlan->document.entries.at(item.entryIndex);
        QString fileName = entry.location.file;
        if (fileName.isEmpty()) {
            fileName = QFileInfo(item.resolvedPath).fileName();
        }
        const TrackId id = matchNmlLibraryTrack(
                candidates, fileName, entry.artist, entry.title, entry.playtimeSeconds);
        if (!id.isValid()) {
            continue;
        }
        const TrackPointer pTrack = pManager->getTrackById(id);
        if (!pTrack) {
            continue;
        }
        if (compareNmlPreparation(*pTrack, entry, kNmlIdenticalTolerance).identical()) {
            item.reuseTrackId = id;
            item.reuseAccepted = true;
        } else {
            item.keptSeparate = true;
        }
    }
}
} // namespace

void proposeNmlReuse(NmlImportPlan* pPlan, TrackCollectionManager* pManager) {
    proposeWithCandidates(pPlan, nmlLibraryCandidates(pManager), pManager);
}

void proposeNmlReuse(const QList<NmlImportPlan*>& plans, TrackCollectionManager* pManager) {
    const QList<NmlLibraryCandidate> candidates = nmlLibraryCandidates(pManager);
    for (NmlImportPlan* pPlan : plans) {
        proposeWithCandidates(pPlan, candidates, pManager);
    }
}

void setNmlReuseAccepted(NmlImportPlan* pPlan, int row, bool accepted) {
    if (row < 0 || row >= pPlan->tracks.size()) {
        return;
    }
    NmlPlanTrack& track = pPlan->tracks[row];
    if (track.reuseTrackId.isValid()) {
        track.reuseAccepted = accepted;
    }
}

} // namespace nml
} // namespace mixxx
