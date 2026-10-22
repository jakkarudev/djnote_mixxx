#include "library/nml/nmlreread.h"

#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSqlQuery>

#include "library/dao/playlistdao.h"
#include "library/dao/trackdao.h"
#include "library/nml/nmlcues.h"
#include "library/nml/nmlkeepreplace.h"
#include "library/nml/nmlpreparation.h"
#include "library/nml/nmlreuse.h"
#include "library/nml/nmlsupport.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "sources/soundsourceproxy.h"
#include "track/globaltrackcache.h"
#include "track/track.h"
#include "track/trackref.h"
#include "util/fileinfo.h"

namespace mixxx {
namespace nml {

namespace {

PlaylistDAO& playlistDaoOf(TrackCollectionManager* pManager) {
    return pManager->internalCollection()->getPlaylistDAO();
}

TrackPointer trackAtPath(TrackCollectionManager* pManager, const QString& path) {
    if (path.isEmpty()) {
        return nullptr;
    }
    if (QFileInfo::exists(path)) {
        return pManager->getTrackByRef(TrackRef::fromFilePath(path));
    }
    // A file that is not there has no canonical location, so TrackRef is not
    // usable: look the (missing) row up by its stored location.
    QSqlQuery query(nmlDatabase(pManager));
    query.prepare(QStringLiteral(
            "SELECT library.id FROM library "
            "JOIN track_locations ON track_locations.id = library.location "
            "WHERE track_locations.location = :location"));
    query.bindValue(QStringLiteral(":location"), mixxx::FileInfo(path).location());
    if (query.exec() && query.next()) {
        return pManager->getTrackById(TrackId(query.value(0)));
    }
    return nullptr;
}

bool isFlaggedMissing(TrackCollectionManager* pManager, TrackId id) {
    QSqlQuery query(nmlDatabase(pManager));
    query.prepare(QStringLiteral(
            "SELECT track_locations.fs_deleted FROM library "
            "JOIN track_locations ON track_locations.id = library.location "
            "WHERE library.id = :id"));
    query.bindValue(QStringLiteral(":id"), id.toVariant());
    return query.exec() && query.next() && query.value(0).toInt() == 1;
}

// Points the library row of `id` at `newPath` (same library id) and clears the
// missing flag. Cached Track objects are relocated too.
bool relinkLibraryTrack(TrackCollectionManager* pManager, TrackId id, const QString& newPath) {
    const mixxx::FileInfo fileInfo(newPath);
    QSqlQuery query(nmlDatabase(pManager));
    query.prepare(QStringLiteral(
            "UPDATE track_locations SET location=:location, directory=:directory, "
            "filename=:filename, filesize=:filesize, fs_deleted=0, needs_verification=0 "
            "WHERE id=(SELECT location FROM library WHERE id=:id)"));
    query.bindValue(QStringLiteral(":location"), fileInfo.location());
    query.bindValue(QStringLiteral(":directory"), fileInfo.locationPath());
    query.bindValue(QStringLiteral(":filename"), fileInfo.fileName());
    query.bindValue(QStringLiteral(":filesize"), fileInfo.sizeInBytes());
    query.bindValue(QStringLiteral(":id"), id.toVariant());
    if (!query.exec()) {
        return false;
    }
    TrackDAO& trackDao = pManager->internalCollection()->getTrackDAO();
    GlobalTrackCacheLocker().relocateCachedTracks(&trackDao);
    trackDao.slotDatabaseTracksChanged(QSet<TrackId>{id});
    return true;
}

// A track that was missing has no stream info: read it from the file so the
// grid and cue positions can be converted.
void refreshStreamInfo(const TrackPointer& pTrack) {
    SoundSourceProxy proxy(pTrack);
    const mixxx::AudioSourcePointer pSource = proxy.openAudioSource();
    if (pSource) {
        pSource->close();
    }
}

QList<TrackId> tracksInOrder(TrackCollectionManager* pManager, int playlistId) {
    QSqlQuery query(nmlDatabase(pManager));
    query.prepare(QStringLiteral(
            "SELECT track_id FROM PlaylistTracks WHERE playlist_id = :id ORDER BY position"));
    query.bindValue(QStringLiteral(":id"), playlistId);
    QList<TrackId> ids;
    if (query.exec()) {
        while (query.next()) {
            ids.append(TrackId(query.value(0)));
        }
    }
    return ids;
}

QList<int> positionsInPlaylist(TrackCollectionManager* pManager, int playlistId) {
    QSqlQuery query(nmlDatabase(pManager));
    query.prepare(QStringLiteral(
            "SELECT position FROM PlaylistTracks WHERE playlist_id = :id"));
    query.bindValue(QStringLiteral(":id"), playlistId);
    QList<int> positions;
    if (query.exec()) {
        while (query.next()) {
            positions.append(query.value(0).toInt());
        }
    }
    return positions;
}

bool itemIsFound(const NmlPlanTrack& item) {
    return item.status == NmlTrackStatus::Found ||
            item.status == NmlTrackStatus::FoundNextToNml ||
            item.status == NmlTrackStatus::FoundInBrowsedFolder;
}

// The library row (if any) a plan item stands for, without writing anything.
// Used for the added/removed counts.
TrackId libraryIdForItem(TrackCollectionManager* pManager, const NmlPlanTrack& item) {
    if (item.status == NmlTrackStatus::MatchedInLibrary && item.accepted &&
            item.libraryTrackId.isValid()) {
        return item.libraryTrackId;
    }
    if (itemIsFound(item)) {
        if (const TrackPointer pTrack = trackAtPath(pManager, item.resolvedPath)) {
            return pTrack->getId();
        }
    }
    if (const TrackPointer pTrack = trackAtPath(pManager, item.recordedPath)) {
        if (!itemIsFound(item) || isFlaggedMissing(pManager, pTrack->getId())) {
            return pTrack->getId();
        }
    }
    if (item.reuseTrackId.isValid() && item.reuseAccepted) {
        return item.reuseTrackId;
    }
    return TrackId();
}

struct Resolved {
    TrackId id;
    bool relinked = false;
};

} // namespace

NmlRereadPlan planNmlReread(int playlistId,
        TrackCollectionManager* pTrackCollectionManager,
        const QString& nmlPathOverride) {
    NmlRereadPlan result;
    result.playlistId = playlistId;
    PlaylistDAO& dao = playlistDaoOf(pTrackCollectionManager);
    const std::optional<SourceNml> sourceNml =
            loadSourceNml(nmlDatabase(pTrackCollectionManager), playlistId);
    if (!sourceNml) {
        result.error = tr("This playlist was not imported from an .nml");
        return result;
    }
    result.sourceNml = *sourceNml;
    if (dao.isPlaylistLocked(playlistId)) {
        result.locked = true;
        result.error = tr("The playlist is locked. Unlock it to re-read the .nml.");
        return result;
    }
    const QString nmlPath = nmlPathOverride.isEmpty() ? sourceNml->nmlPath : nmlPathOverride;
    if (!QFileInfo::exists(nmlPath)) {
        result.nmlFileMissing = true;
        result.error = tr("The .nml file was not found at %1").arg(nmlPath);
        return result;
    }
    // Find the playlist: UUID first, then name.
    NmlImportPlan probe = planNmlImport(nmlPath, dao, -1);
    if (!probe.error.isEmpty()) {
        result.error = probe.error;
        return result;
    }
    int index = -1;
    const auto& playlists = probe.document.playlists;
    if (!sourceNml->uuid.isEmpty()) {
        for (int i = 0; i < static_cast<int>(playlists.size()); ++i) {
            if (playlists.at(i).uuid == sourceNml->uuid) {
                index = i;
                break;
            }
        }
    }
    if (index < 0) {
        for (int i = 0; i < static_cast<int>(playlists.size()); ++i) {
            if (playlists.at(i).name == sourceNml->name) {
                index = i;
                break;
            }
        }
    }
    if (index < 0) {
        result.error = tr("The playlist \"%1\" is not in the .nml any more").arg(sourceNml->name);
        return result;
    }
    result.plan = selectNmlPlaylist(probe, index);
    result.plan.nmlPath = nmlPath;
    updateNmlPlanComparisons(&result.plan, pTrackCollectionManager);
    // Rows whose file is a copy of a library track may reuse it, as in the import.
    proposeNmlReuse(&result.plan, pTrackCollectionManager);
    return result;
}

bool nmlRereadEntryMissing(
        const NmlRereadPlan& plan, int row, TrackCollectionManager* pTrackCollectionManager) {
    if (row < 0 || row >= plan.plan.tracks.size()) {
        return false;
    }
    const TrackPointer pTrack = trackAtPath(pTrackCollectionManager, plan.plan.tracks.at(row).recordedPath);
    if (!pTrack || !isFlaggedMissing(pTrackCollectionManager, pTrack->getId())) {
        return false;
    }
    const QList<TrackId> ids = tracksInOrder(pTrackCollectionManager, plan.playlistId);
    return ids.contains(pTrack->getId());
}

NmlRereadCounts nmlRereadCounts(
        const NmlRereadPlan& plan, TrackCollectionManager* pTrackCollectionManager) {
    QHash<QString, int> current;
    for (const TrackId id : tracksInOrder(pTrackCollectionManager, plan.playlistId)) {
        ++current[id.toString()];
    }
    QHash<QString, int> target;
    for (int i = 0; i < plan.plan.tracks.size(); ++i) {
        const NmlPlanTrack& item = plan.plan.tracks.at(i);
        const TrackId id = libraryIdForItem(pTrackCollectionManager, item);
        // A file that is not in the library yet is a new track, one per file.
        const QString key = id.isValid()
                ? id.toString()
                : QStringLiteral("new:") +
                        (item.resolvedPath.isEmpty() ? item.recordedPath : item.resolvedPath);
        ++target[key];
    }
    NmlRereadCounts counts;
    for (auto it = target.constBegin(); it != target.constEnd(); ++it) {
        counts.added += std::max(0, it.value() - current.value(it.key(), 0));
    }
    for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
        counts.removed += std::max(0, it.value() - target.value(it.key(), 0));
    }
    return counts;
}

NmlRereadResult applyNmlReread(const NmlRereadPlan& rereadPlan,
        NmlRereadMode mode,
        TrackCollectionManager* pManager) {
    NmlRereadResult result;
    if (!rereadPlan.error.isEmpty()) {
        result.error = rereadPlan.error;
        return result;
    }
    PlaylistDAO& dao = playlistDaoOf(pManager);
    const int playlistId = rereadPlan.playlistId;
    if (dao.isPlaylistLocked(playlistId)) {
        result.error = tr("The playlist is locked. Unlock it to re-read the .nml.");
        return result;
    }
    const NmlImportPlan& plan = rereadPlan.plan;
    const QList<TrackId> currentIds = tracksInOrder(pManager, playlistId);
    const QSet<TrackId> currentSet(currentIds.constBegin(), currentIds.constEnd());
    const bool replace = mode == NmlRereadMode::Replace;

    if (replace) {
        const NmlRereadCounts counts = nmlRereadCounts(rereadPlan, pManager);
        result.added = counts.added;
        result.removed = counts.removed;
    }

    QList<TrackId> targetIds;
    QSet<TrackId> handled; // a repeat of a track is prepared once
    QHash<TrackId, TrackId> swaps; // Keep: missing track in the list -> accepted library match
    for (const NmlPlanTrack& item : plan.tracks) {
        const QString label = nmlPlanTrackLabel(item);
        const NmlEntry* pEntry = item.entryIndex >= 0
                ? &plan.document.entries.at(item.entryIndex)
                : nullptr;
        TrackId id;
        const bool matched = item.status == NmlTrackStatus::MatchedInLibrary && item.accepted &&
                item.libraryTrackId.isValid() &&
                pManager->getTrackById(item.libraryTrackId);
        if (matched) {
            id = item.libraryTrackId;
            if (!replace) {
                // Keep: the match stands in for the missing track already in the list.
                const TrackPointer pOld = trackAtPath(pManager, item.recordedPath);
                if (!pOld || pOld->getId() == id || !currentSet.contains(pOld->getId())) {
                    continue;
                }
                swaps.insert(pOld->getId(), id);
            }
            if (pEntry && !handled.contains(id)) {
                const TrackPointer pTrack = pManager->getTrackById(id);
                const NmlKeepReplaceOutcome outcome =
                        applyNmlToExistingTrack(pTrack.get(), *pEntry, plan.preparationMode);
                result.existingTracksUpdated += outcome.changed ? 1 : 0;
                result.droppedCues += outcome.droppedCues;
                if (outcome.lockedGridKept) {
                    result.lockedGridTracks.append(label);
                }
            }
        } else if (!itemIsFound(item)) {
            // Still missing (or an unticked library proposal).
            if (const TrackPointer pTrack = trackAtPath(pManager, item.recordedPath)) {
                id = pTrack->getId();
            } else if (replace) {
                if (!ensureNmlMissingTrack(pManager, item, &id)) {
                    continue;
                }
            }
            if (!id.isValid()) {
                continue;
            }
            if (replace || currentSet.contains(id)) {
                result.missingTracks.append(label);
            }
        } else {
            bool relinked = false;
            bool isNew = false;
            bool reused = false;
            bool usedExisting = false;
            TrackPointer pTrack = trackAtPath(pManager, item.resolvedPath);
            if (pTrack) {
                if (isFlaggedMissing(pManager, pTrack->getId())) {
                    relinked = relinkLibraryTrack(pManager, pTrack->getId(), item.resolvedPath);
                    if (!relinked) {
                        result.failedRelinks.append(label);
                    }
                } else {
                    // The found file is already a library track. A missing track
                    // at the recorded path stands in for it in the list.
                    const TrackPointer pOld = trackAtPath(pManager, item.recordedPath);
                    if (pOld && pOld->getId() != pTrack->getId() &&
                            isFlaggedMissing(pManager, pOld->getId())) {
                        if (replace) {
                            usedExisting = true;
                        } else if (currentSet.contains(pOld->getId())) {
                            swaps.insert(pOld->getId(), pTrack->getId());
                            usedExisting = true;
                        }
                    }
                }
            } else {
                TrackPointer pMissing = trackAtPath(pManager, item.recordedPath);
                if (pMissing && isFlaggedMissing(pManager, pMissing->getId())) {
                    const TrackId missingId = pMissing->getId();
                    // Drop our reference first: the cache does not relocate a track
                    // that had no canonical location, so a fresh object is loaded.
                    pMissing.reset();
                    if (relinkLibraryTrack(pManager, missingId, item.resolvedPath)) {
                        relinked = true;
                        pTrack = pManager->getTrackById(missingId);
                    } else {
                        result.failedRelinks.append(label);
                    }
                } else if (replace && item.reuseTrackId.isValid() && item.reuseAccepted &&
                        pManager->getTrackById(item.reuseTrackId)) {
                    // An identical copy of a library track is that track.
                    pTrack = pManager->getTrackById(item.reuseTrackId);
                    reused = true;
                    if (!handled.contains(pTrack->getId())) {
                        result.reusedTracks.append(label);
                    }
                } else if (replace) {
                    bool alreadyInLibrary = false;
                    pTrack = pManager->getOrAddTrack(
                            TrackRef::fromFilePath(item.resolvedPath), &alreadyInLibrary);
                    isNew = pTrack && !alreadyInLibrary;
                }
            }
            if (!pTrack || !pTrack->getId().isValid()) {
                continue;
            }
            id = pTrack->getId();
            if (!replace && !currentSet.contains(id) && !usedExisting) {
                continue; // Keep: only tracks already in the list are looked at
            }
            if (!reused && !handled.contains(id) && pEntry) {
                if (isNew) {
                    ++result.newLibraryTracks;
                    applyNmlPreparation(pTrack.get(), *pEntry);
                    result.droppedCues += applyNmlCues(pTrack.get(), *pEntry);
                    applyNmlColorAndRating(pTrack.get(), *pEntry);
                } else if (relinked || replace || usedExisting) {
                    if (relinked) {
                        refreshStreamInfo(pTrack);
                    }
                    const NmlKeepReplaceOutcome outcome =
                            applyNmlToExistingTrack(pTrack.get(), *pEntry, plan.preparationMode);
                    result.existingTracksUpdated += outcome.changed ? 1 : 0;
                    result.droppedCues += outcome.droppedCues;
                    if (outcome.lockedGridKept) {
                        result.lockedGridTracks.append(label);
                    }
                }
            }
            if (relinked && !handled.contains(id)) {
                result.relinked.append(label);
            }
            if (usedExisting && !handled.contains(id)) {
                result.usedExistingTracks.append(label);
            }
        }
        handled.insert(id);
        targetIds.append(id);
    }

    const auto rewritePlaylist = [&](const QList<TrackId>& ids) {
        const QList<int> positions = positionsInPlaylist(pManager, playlistId);
        if (!positions.isEmpty()) {
            dao.removeTracksFromPlaylist(playlistId, positions);
        }
        if (!ids.isEmpty() && !dao.appendTracksToPlaylist(ids, playlistId)) {
            result.error = tr("Could not add the tracks to the playlist");
        }
    };
    if (replace) {
        rewritePlaylist(targetIds);
        result.tracksInPlaylist = targetIds.size();
    } else {
        if (!swaps.isEmpty()) {
            QList<TrackId> swapped = currentIds;
            for (TrackId& id : swapped) {
                id = swaps.value(id, id);
            }
            rewritePlaylist(swapped);
        }
        result.tracksInPlaylist = currentIds.size();
    }

    // Remember where the .nml is now, and refresh the NML playlist's identity.
    SourceNml sourceNml = rereadPlan.sourceNml;
    sourceNml.nmlPath = plan.nmlPath;
    if (plan.playlistIndex >= 0 && plan.playlistIndex < plan.document.playlists.size()) {
        sourceNml.uuid = plan.document.playlists.at(plan.playlistIndex).uuid;
        sourceNml.name = plan.document.playlists.at(plan.playlistIndex).name;
    }
    saveSourceNml(nmlDatabase(pManager), sourceNml);
    return result;
}

QString nmlRereadSummary(
        const QString& playlistName, NmlRereadMode mode, const NmlRereadResult& r) {
    QString text;
    if (mode == NmlRereadMode::Replace) {
        text = tr("The playlist \"%1\" now has the .nml's %2 tracks (%3 added, %4 removed).")
                       .arg(playlistName,
                               QString::number(r.tracksInPlaylist),
                               QString::number(r.added),
                               QString::number(r.removed));
    } else {
        text = tr("The playlist \"%1\" was kept as it is.").arg(playlistName);
    }
    text += QStringLiteral("\n\n") +
            tr("%1 missing tracks were found again and relinked.")
                    .arg(QString::number(r.relinked.size()));
    if (!r.relinked.isEmpty()) {
        text += QStringLiteral("\n") + r.relinked.join(QLatin1Char('\n'));
    }
    if (!r.usedExistingTracks.isEmpty()) {
        text += QStringLiteral("\n\n") +
                tr("%1 missing tracks were found again; the file is already in the library, "
                   "so the playlist uses the track already in the library:")
                        .arg(QString::number(r.usedExistingTracks.size())) +
                QStringLiteral("\n") + r.usedExistingTracks.join(QLatin1Char('\n'));
    }
    if (!r.failedRelinks.isEmpty()) {
        text += QStringLiteral("\n\n") +
                tr("%1 tracks were found again but could not be relinked:")
                        .arg(QString::number(r.failedRelinks.size())) +
                QStringLiteral("\n") + r.failedRelinks.join(QLatin1Char('\n'));
    }
    if (!r.reusedTracks.isEmpty()) {
        text += QStringLiteral("\n\n") +
                tr("%1 tracks are copies of tracks already in the library and use those:")
                        .arg(QString::number(r.reusedTracks.size())) +
                QStringLiteral("\n") + r.reusedTracks.join(QLatin1Char('\n'));
    }
    if (!r.missingTracks.isEmpty()) {
        text += QStringLiteral("\n\n") +
                tr("%1 tracks are still missing:").arg(QString::number(r.missingTracks.size())) +
                QStringLiteral("\n") + r.missingTracks.join(QLatin1Char('\n'));
    }
    if (!r.lockedGridTracks.isEmpty()) {
        text += QStringLiteral("\n\n") +
                tr("%1 tracks have a BPM-locked grid; their grid was not replaced:")
                        .arg(QString::number(r.lockedGridTracks.size())) +
                QStringLiteral("\n") + r.lockedGridTracks.join(QLatin1Char('\n'));
    }
    if (r.droppedCues > 0) {
        text += QStringLiteral("\n\n") +
                tr("%1 cues were not imported.").arg(QString::number(r.droppedCues));
    }
    return text;
}

} // namespace nml
} // namespace mixxx
