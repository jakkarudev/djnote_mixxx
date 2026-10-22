#include "library/nml/nmlimport.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "library/dao/playlistdao.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "track/track.h"
#include "track/trackref.h"

namespace mixxx {
namespace nml {

namespace {

// DJNote: flags the library row of `pTrack` as missing (track_locations.fs_deleted)
// and gives it its NML title/artist; a file that is not there can't be read.
bool addMissingTrack(TrackCollectionManager* pManager,
        const NmlPlanTrack& item,
        TrackId* pId) {
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
    }
    *pId = pTrack->getId();
    QSqlQuery query(pManager->internalCollection()->database());
    query.prepare(QStringLiteral(
            "UPDATE track_locations SET fs_deleted=1 WHERE id="
            "(SELECT location FROM library WHERE id=:id)"));
    query.bindValue(QStringLiteral(":id"), pTrack->getId().toVariant());
    return query.exec();
}

} // namespace

QString nextFreePlaylistName(const PlaylistDAO& dao, const QString& base) {
    QString name = base;
    int i = 1;
    while (dao.getPlaylistIdFromName(name) != kInvalidPlaylistId) {
        name = base + QChar(' ') + QString::number(i);
        ++i;
    }
    return name;
}

NmlImportPlan planNmlImport(const QString& nmlPath,
        const PlaylistDAO& dao,
        int playlistIndex) {
    NmlImportPlan plan;
    plan.nmlPath = nmlPath;
    plan.playlistIndex = playlistIndex;
    QString error;
    if (!plan.document.readFile(nmlPath, &error)) {
        plan.error = error;
        return plan;
    }
    if (playlistIndex < 0 || playlistIndex >= plan.document.playlists.size()) {
        plan.error = QStringLiteral("The file contains no playlist");
        return plan;
    }
    const QFileInfo nmlInfo(nmlPath);
    plan.defaultName = nextFreePlaylistName(dao, nmlInfo.completeBaseName());

    const NmlPlaylist& playlist = plan.document.playlists.at(playlistIndex);
    for (const QString& key : playlist.primaryKeys) {
        NmlPlanTrack track;
        track.primaryKey = key;
        track.entryIndex = plan.document.entryIndexForPrimaryKey(key);
        QString recordedPath;
        QString fileName;
        if (track.entryIndex >= 0) {
            const NmlEntry& entry = plan.document.entries.at(track.entryIndex);
            track.title = entry.title;
            track.artist = entry.artist;
            recordedPath = entry.location.recordedPath();
            fileName = entry.location.file;
        } else {
            // No collection entry: fall back to the key itself.
            recordedPath = fromTraktorSeparators(key);
            fileName = QFileInfo(recordedPath).fileName();
        }
        track.recordedPath = recordedPath;
        if (!recordedPath.isEmpty() && QFileInfo::exists(recordedPath)) {
            track.status = NmlTrackStatus::Found;
            track.resolvedPath = recordedPath;
        } else {
            const QString nextTo = nmlInfo.absoluteDir().filePath(fileName);
            if (!fileName.isEmpty() && QFileInfo::exists(nextTo)) {
                track.status = NmlTrackStatus::FoundNextToNml;
                track.resolvedPath = nextTo;
            }
        }
        plan.tracks.append(track);
    }
    return plan;
}

NmlImportResult applyNmlImport(const NmlImportPlan& plan,
        const QString& playlistName,
        TrackCollectionManager* pTrackCollectionManager) {
    NmlImportResult result;
    if (!plan.error.isEmpty()) {
        result.error = plan.error;
        return result;
    }
    PlaylistDAO& dao = pTrackCollectionManager->internalCollection()->getPlaylistDAO();
    const QString name = playlistName.trimmed();
    if (name.isEmpty()) {
        result.error = QStringLiteral("The playlist name is empty");
        return result;
    }
    if (dao.getPlaylistIdFromName(name) != kInvalidPlaylistId) {
        result.error = QStringLiteral("A playlist named \"%1\" already exists").arg(name);
        return result;
    }

    QList<TrackId> ids;
    for (const NmlPlanTrack& item : plan.tracks) {
        const QString label = item.title.isEmpty()
                ? item.primaryKey
                : (item.artist.isEmpty() ? item.title : item.artist + QStringLiteral(" - ") + item.title);
        if (item.status == NmlTrackStatus::NotFound) {
            TrackId missingId;
            if (addMissingTrack(pTrackCollectionManager, item, &missingId)) {
                result.missingTracks.append(label);
                ids.append(missingId);
            } else {
                result.skipped.append(label);
            }
            continue;
        }
        bool alreadyInLibrary = false;
        const TrackPointer pTrack = pTrackCollectionManager->getOrAddTrack(
                TrackRef::fromFilePath(item.resolvedPath), &alreadyInLibrary);
        if (!pTrack || !pTrack->getId().isValid()) {
            result.skipped.append(label);
            continue;
        }
        if (!alreadyInLibrary) {
            ++result.newLibraryTracks;
        }
        ids.append(pTrack->getId());
    }

    const int playlistId = dao.createPlaylist(name);
    if (playlistId == kInvalidPlaylistId) {
        result.error = QStringLiteral("Could not create the playlist");
        return result;
    }
    if (!ids.isEmpty() && !dao.appendTracksToPlaylist(ids, playlistId)) {
        result.error = QStringLiteral("Could not add the tracks to the playlist");
    }
    result.playlistId = playlistId;
    result.tracksInPlaylist = ids.size();
    return result;
}

} // namespace nml
} // namespace mixxx
