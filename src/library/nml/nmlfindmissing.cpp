#include "library/nml/nmlfindmissing.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>

#include <cmath>

#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"

namespace mixxx {
namespace nml {

namespace {
bool sameText(const QString& a, const QString& b) {
    return a.trimmed().compare(b.trimmed(), Qt::CaseInsensitive) == 0;
}
} // namespace

int browseNmlMissingInFolder(NmlImportPlan* pPlan, const QString& folder) {
    int found = 0;
    const QDir dir(folder);
    for (NmlPlanTrack& track : pPlan->tracks) {
        if (track.status != NmlTrackStatus::NotFound) {
            continue;
        }
        QString fileName;
        if (track.entryIndex >= 0) {
            fileName = pPlan->document.entries.at(track.entryIndex).location.file;
        }
        if (fileName.isEmpty()) {
            fileName = QFileInfo(track.recordedPath).fileName();
        }
        if (fileName.isEmpty()) {
            continue;
        }
        const QString candidate = dir.filePath(fileName);
        if (QFileInfo(candidate).isFile()) {
            track.status = NmlTrackStatus::FoundInBrowsedFolder;
            track.resolvedPath = candidate;
            ++found;
        }
    }
    return found;
}

QList<NmlLibraryCandidate> nmlLibraryCandidates(TrackCollectionManager* pManager) {
    QList<NmlLibraryCandidate> candidates;
    QSqlQuery query(pManager->internalCollection()->database());
    if (!query.exec(QStringLiteral(
                "SELECT library.id, track_locations.filename, library.artist, "
                "library.title, library.duration FROM library "
                "JOIN track_locations ON track_locations.id = library.location "
                "WHERE library.mixxx_deleted = 0 AND track_locations.fs_deleted = 0"))) {
        return candidates;
    }
    while (query.next()) {
        NmlLibraryCandidate c;
        c.id = TrackId(query.value(0));
        c.fileName = query.value(1).toString();
        c.artist = query.value(2).toString();
        c.title = query.value(3).toString();
        c.durationSeconds = query.value(4).toDouble();
        candidates.append(c);
    }
    return candidates;
}

TrackId matchNmlLibraryTrack(const QList<NmlLibraryCandidate>& candidates,
        const QString& fileName,
        const QString& artist,
        const QString& title,
        double playtimeSeconds,
        const QSet<TrackId>& exclude) {
    if (playtimeSeconds <= 0.0) {
        return TrackId();
    }
    // Rule 1 (file name), then rule 2 (artist + title).
    for (int rule = 1; rule <= 2; ++rule) {
        if (rule == 1 && fileName.trimmed().isEmpty()) {
            continue;
        }
        if (rule == 2 && (artist.trimmed().isEmpty() || title.trimmed().isEmpty())) {
            continue;
        }
        TrackId best;
        double bestDiff = 0.0;
        for (const NmlLibraryCandidate& c : candidates) {
            if (exclude.contains(c.id)) {
                continue;
            }
            const bool nameOk = rule == 1
                    ? sameText(c.fileName, fileName)
                    : (sameText(c.artist, artist) && sameText(c.title, title));
            if (!nameOk) {
                continue;
            }
            const double diff = std::abs(c.durationSeconds - playtimeSeconds);
            if (diff > kNmlLibraryMatchToleranceSeconds) {
                continue;
            }
            if (!best.isValid() || diff < bestDiff) {
                best = c.id;
                bestDiff = diff;
            }
        }
        if (best.isValid()) {
            return best;
        }
    }
    return TrackId();
}

int findNmlMissingInLibrary(NmlImportPlan* pPlan,
        const QList<NmlLibraryCandidate>& candidates) {
    int proposed = 0;
    for (NmlPlanTrack& track : pPlan->tracks) {
        if (track.status != NmlTrackStatus::NotFound || track.entryIndex < 0) {
            continue;
        }
        const NmlEntry& entry = pPlan->document.entries.at(track.entryIndex);
        QString fileName = entry.location.file;
        if (fileName.isEmpty()) {
            fileName = QFileInfo(track.recordedPath).fileName();
        }
        const TrackId id = matchNmlLibraryTrack(
                candidates, fileName, entry.artist, entry.title, entry.playtimeSeconds);
        if (id.isValid()) {
            track.status = NmlTrackStatus::MatchedInLibrary;
            track.libraryTrackId = id;
            track.accepted = true;
            ++proposed;
        }
    }
    return proposed;
}

int findNmlMissingInLibrary(NmlImportPlan* pPlan, TrackCollectionManager* pManager) {
    return findNmlMissingInLibrary(pPlan, nmlLibraryCandidates(pManager));
}

void setNmlMatchAccepted(NmlImportPlan* pPlan, int row, bool accepted) {
    if (row < 0 || row >= pPlan->tracks.size()) {
        return;
    }
    NmlPlanTrack& track = pPlan->tracks[row];
    if (track.status == NmlTrackStatus::MatchedInLibrary) {
        track.accepted = accepted;
    }
}

int browseNmlMissingInFolder(const QList<NmlImportPlan*>& plans, const QString& folder) {
    int found = 0;
    for (NmlImportPlan* pPlan : plans) {
        found += browseNmlMissingInFolder(pPlan, folder);
    }
    return found;
}

int findNmlMissingInLibrary(const QList<NmlImportPlan*>& plans, TrackCollectionManager* pManager) {
    const QList<NmlLibraryCandidate> candidates = nmlLibraryCandidates(pManager);
    int proposed = 0;
    for (NmlImportPlan* pPlan : plans) {
        proposed += findNmlMissingInLibrary(pPlan, candidates);
    }
    return proposed;
}

} // namespace nml
} // namespace mixxx
