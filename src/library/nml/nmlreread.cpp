#include "library/nml/nmlreread.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSqlQuery>

#include "library/dao/playlistdao.h"
#include "library/dao/trackdao.h"
#include "library/nml/nmlcues.h"
#include "library/nml/nmlkeepreplace.h"
#include "library/nml/nmlpreparation.h"
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

QString tr(const char* text) {
    return QCoreApplication::translate("NmlReread", text);
}

QSqlDatabase databaseOf(TrackCollectionManager* pManager) {
    return pManager->internalCollection()->database();
}

PlaylistDAO& playlistDaoOf(TrackCollectionManager* pManager) {
    return pManager->internalCollection()->getPlaylistDAO();
}

QString labelOf(const NmlPlanTrack& item) {
    if (item.title.isEmpty()) {
        return item.primaryKey;
    }
    return item.artist.isEmpty() ? item.title
                                 : item.artist + QStringLiteral(" - ") + item.title;
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
    QSqlQuery query(databaseOf(pManager));
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
    QSqlQuery query(databaseOf(pManager));
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
    QSqlQuery query(databaseOf(pManager));
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

// Flags the library row of a not-found item as missing (or finds the row that
// already is). Mirrors the import's missing-track rule.
bool ensureMissingTrack(
        TrackCollectionManager* pManager, const NmlPlanTrack& item, TrackId* pId) {
    if (item.recordedPath.isEmpty()) {
        return false;
    }
    bool alreadyInLibrary = false;
    const TrackPointer pTrack = pManager->getOrAddTrack(
            TrackRef::fromFilePath(item.recordedPath), &alreadyInLibrary);
    if (!pTrack || !pTrack->getId().isValid()) {
        return false;
    }
    if (!alreadyInLibrary) {
        if (!item.title.isEmpty()) {
            pTrack->setTitle(item.title);
        }
        if (!item.artist.isEmpty()) {
            pTrack->setArtist(item.artist);
        }
        QSqlQuery query(databaseOf(pManager));
        query.prepare(QStringLiteral(
                "UPDATE track_locations SET fs_deleted=1 WHERE id="
                "(SELECT location FROM library WHERE id=:id)"));
        query.bindValue(QStringLiteral(":id"), pTrack->getId().toVariant());
        if (!query.exec()) {
            return false;
        }
    }
    *pId = pTrack->getId();
    return true;
}

QList<TrackId> tracksInOrder(TrackCollectionManager* pManager, int playlistId) {
    QSqlQuery query(databaseOf(pManager));
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
    QSqlQuery query(databaseOf(pManager));
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
    const std::optional<NmlSourceLink> link =
            loadNmlSourceLink(databaseOf(pTrackCollectionManager), playlistId);
    if (!link) {
        result.error = tr("This playlist was not imported from a Traktor .nml");
        return result;
    }
    result.link = *link;
    if (dao.isPlaylistLocked(playlistId)) {
        result.locked = true;
        result.error = tr("The playlist is locked. Unlock it to re-read the .nml.");
        return result;
    }
    const QString nmlPath = nmlPathOverride.isEmpty() ? link->nmlPath : nmlPathOverride;
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
    if (!link->uuid.isEmpty()) {
        for (int i = 0; i < static_cast<int>(playlists.size()); ++i) {
            if (playlists.at(i).uuid == link->uuid) {
                index = i;
                break;
            }
        }
    }
    if (index < 0) {
        for (int i = 0; i < static_cast<int>(playlists.size()); ++i) {
            if (playlists.at(i).name == link->name) {
                index = i;
                break;
            }
        }
    }
    if (index < 0) {
        result.error = tr("The playlist \"%1\" is not in the .nml any more").arg(link->name);
        return result;
    }
    result.plan = selectNmlPlaylist(probe, index);
    result.plan.nmlPath = nmlPath;
    updateNmlPlanComparisons(&result.plan, pTrackCollectionManager);
    return result;
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
    for (const NmlPlanTrack& item : plan.tracks) {
        const QString label = labelOf(item);
        const NmlEntry* pEntry = item.entryIndex >= 0
                ? &plan.document.entries.at(item.entryIndex)
                : nullptr;
        TrackId id;
        const bool matched = item.status == NmlTrackStatus::MatchedInLibrary && item.accepted &&
                item.libraryTrackId.isValid() &&
                pManager->getTrackById(item.libraryTrackId);
        if (matched) {
            id = item.libraryTrackId;
            if (replace && pEntry && !handled.contains(id)) {
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
                if (!ensureMissingTrack(pManager, item, &id)) {
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
            TrackPointer pTrack = trackAtPath(pManager, item.resolvedPath);
            if (pTrack) {
                if (isFlaggedMissing(pManager, pTrack->getId())) {
                    relinked = relinkLibraryTrack(pManager, pTrack->getId(), item.resolvedPath);
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
            if (!replace && !currentSet.contains(id)) {
                continue; // Keep: only tracks already in the list are looked at
            }
            if (!handled.contains(id) && pEntry) {
                if (isNew) {
                    ++result.newLibraryTracks;
                    applyNmlPreparation(pTrack.get(), *pEntry);
                    result.droppedCues += applyNmlCues(pTrack.get(), *pEntry);
                    applyNmlColorAndRating(pTrack.get(), *pEntry);
                } else if (relinked || replace) {
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
        }
        handled.insert(id);
        targetIds.append(id);
    }

    if (replace) {
        const QList<int> positions = positionsInPlaylist(pManager, playlistId);
        if (!positions.isEmpty()) {
            dao.removeTracksFromPlaylist(playlistId, positions);
        }
        if (!targetIds.isEmpty() && !dao.appendTracksToPlaylist(targetIds, playlistId)) {
            result.error = tr("Could not add the tracks to the playlist");
        }
        result.tracksInPlaylist = targetIds.size();
    } else {
        result.tracksInPlaylist = currentIds.size();
    }

    // Remember where the .nml is now, and refresh the NML playlist's identity.
    NmlSourceLink link = rereadPlan.link;
    link.nmlPath = plan.nmlPath;
    if (plan.playlistIndex >= 0 && plan.playlistIndex < plan.document.playlists.size()) {
        link.uuid = plan.document.playlists.at(plan.playlistIndex).uuid;
        link.name = plan.document.playlists.at(plan.playlistIndex).name;
    }
    saveNmlSourceLink(databaseOf(pManager), link);
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
