#include "library/nml/nmlimport.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlpreparation.h"
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

namespace {
void fillPlanTracks(NmlImportPlan* pPlan, int playlistIndex) {
    NmlImportPlan& plan = *pPlan;
    plan.playlistIndex = playlistIndex;
    plan.tracks.clear();
    const QFileInfo nmlInfo(plan.nmlPath);
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
}
} // namespace

QList<int> importableNmlPlaylists(const NmlDocument& document) {
    QList<int> others;
    int preparation = -1;
    for (int i = 0; i < document.playlists.size(); ++i) {
        const QString& name = document.playlists.at(i).name;
        if (name.startsWith(QLatin1Char('_'))) {
            continue;
        }
        if (name == QLatin1String("Preparation")) {
            preparation = i;
            continue;
        }
        others.append(i);
    }
    if (others.isEmpty() && preparation >= 0) {
        others.append(preparation);
    }
    return others;
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
    if (playlistIndex < 0) {
        const QList<int> importable = importableNmlPlaylists(plan.document);
        playlistIndex = importable.isEmpty() ? -1 : importable.first();
    }
    if (playlistIndex < 0 || playlistIndex >= plan.document.playlists.size()) {
        plan.error = QStringLiteral("The file contains no playlist");
        return plan;
    }
    plan.defaultName = nextFreePlaylistName(dao, QFileInfo(nmlPath).completeBaseName());
    fillPlanTracks(&plan, playlistIndex);
    return plan;
}

NmlImportPlan selectNmlPlaylist(const NmlImportPlan& plan, int playlistIndex) {
    NmlImportPlan selected = plan;
    if (playlistIndex < 0 || playlistIndex >= plan.document.playlists.size()) {
        selected.error = QStringLiteral("The file contains no playlist");
        selected.tracks.clear();
        return selected;
    }
    fillPlanTracks(&selected, playlistIndex);
    return selected;
}

QList<NmlPlaylistChoice> nmlPlaylistChoices(const NmlImportPlan& plan, const PlaylistDAO& dao) {
    QList<NmlPlaylistChoice> choices;
    const QList<int> importable = importableNmlPlaylists(plan.document);
    QSet<QString> taken;
    for (int index : importable) {
        const NmlPlaylist& playlist = plan.document.playlists.at(index);
        NmlPlaylistChoice choice;
        choice.playlistIndex = index;
        choice.trackCount = static_cast<int>(playlist.primaryKeys.size());
        if (importable.size() == 1) {
            choice.name = plan.defaultName;
        } else {
            QString base = playlist.name.trimmed();
            if (base.isEmpty()) {
                base = QFileInfo(plan.nmlPath).completeBaseName();
            }
            QString name = nextFreePlaylistName(dao, base);
            // Also avoid the names given to earlier rows.
            for (int i = 1; taken.contains(name); ++i) {
                name = nextFreePlaylistName(dao, base + QChar(' ') + QString::number(i));
            }
            choice.name = name;
        }
        taken.insert(choice.name);
        choices.append(choice);
    }
    return choices;
}

QList<NmlImportPlan> nmlPlansForChoices(
        const NmlImportPlan& plan, const QList<NmlPlaylistChoice>& choices) {
    QList<NmlImportPlan> plans;
    for (const NmlPlaylistChoice& choice : choices) {
        plans.append(selectNmlPlaylist(plan, choice.playlistIndex));
    }
    return plans;
}

NmlMultiImportResult applyNmlImportPlaylists(const QList<NmlImportPlan>& plans,
        const QList<NmlPlaylistChoice>& choices,
        TrackCollectionManager* pTrackCollectionManager) {
    NmlMultiImportResult multi;
    const PlaylistDAO& dao = pTrackCollectionManager->internalCollection()->getPlaylistDAO();
    for (int i = 0; i < choices.size() && i < plans.size(); ++i) {
        const NmlPlaylistChoice& choice = choices.at(i);
        if (!choice.selected) {
            continue;
        }
        // Earlier rows are already created, so this also separates equal names.
        const QString base = choice.name.trimmed();
        const QString name = base.isEmpty() ? base : nextFreePlaylistName(dao, base);
        multi.playlistNames.append(name);
        multi.results.append(applyNmlImport(plans.at(i), name, pTrackCollectionManager));
    }
    return multi;
}

NmlMultiImportResult applyNmlImportPlaylists(const NmlImportPlan& plan,
        const QList<NmlPlaylistChoice>& choices,
        TrackCollectionManager* pTrackCollectionManager) {
    if (!plan.error.isEmpty()) {
        NmlMultiImportResult multi;
        multi.error = plan.error;
        return multi;
    }
    return applyNmlImportPlaylists(
            nmlPlansForChoices(plan, choices), choices, pTrackCollectionManager);
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
        // DJNote (#111): an accepted library match puts that track in the Playlist;
        // an unticked one is applied like a missing row. The existing track's
        // preparation is the keep/replace rule's business, not touched here.
        if (item.status == NmlTrackStatus::MatchedInLibrary && item.accepted &&
                pTrackCollectionManager->getTrackById(item.libraryTrackId)) {
            ids.append(item.libraryTrackId);
            ++result.matchedLibraryTracks;
            result.matchedLibraryLabels.append(label);
            continue;
        }
        if (item.status == NmlTrackStatus::NotFound ||
                item.status == NmlTrackStatus::MatchedInLibrary) {
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
            // Preparation (grid, key, lock) applies to tracks new to the
            // library only; existing ones belong to the keep/replace ticket.
            if (item.entryIndex >= 0) {
                applyNmlPreparation(pTrack.get(), plan.document.entries.at(item.entryIndex));
            }
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
