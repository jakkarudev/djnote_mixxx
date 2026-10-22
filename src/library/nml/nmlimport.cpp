#include "library/nml/nmlimport.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include "library/dao/playlistdao.h"
#include "library/nml/nmlcues.h"
#include "library/nml/nmlpreparation.h"
#include "library/nml/nmlsupport.h"
#include "library/nml/sourcenml.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "track/track.h"
#include "track/trackref.h"

namespace mixxx {
namespace nml {

QString nextFreePlaylistName(const PlaylistDAO& dao, const QString& base) {
    QString name = base;
    int i = 1;
    while (dao.getPlaylistIdFromName(name) != kInvalidPlaylistId) {
        name = base + QChar(' ') + QString::number(i);
        ++i;
    }
    return name;
}

QString nmlStatusText(NmlTrackStatus status) {
    switch (status) {
    case NmlTrackStatus::Found:
        return tr("found");
    case NmlTrackStatus::FoundNextToNml:
        return tr("found next to the .nml");
    case NmlTrackStatus::FoundInBrowsedFolder:
        return tr("found in browsed folder");
    case NmlTrackStatus::MatchedInLibrary:
        return tr("matched in library");
    case NmlTrackStatus::NotFound:
        break;
    }
    return tr("missing");
}

QString nmlRowStatusText(const NmlPlanTrack& track) {
    if (track.reuseTrackId.isValid()) {
        return track.reuseAccepted
                ? tr("reuse existing track")
                : tr("kept separate: untick of reuse existing track");
    }
    if (track.keptSeparate) {
        return tr("%1, kept separate: preparation differs").arg(nmlStatusText(track.status));
    }
    return nmlStatusText(track.status);
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
// DJNote
void applyKeepReplace(Track* pTrack,
        const NmlImportPlan& plan,
        const NmlPlanTrack& item,
        const QString& label,
        NmlImportResult* pResult) {
    if (!pTrack || item.entryIndex < 0) {
        return;
    }
    const NmlEntry& entry = plan.document.entries.at(item.entryIndex);
    const NmlKeepReplaceOutcome outcome =
            applyNmlToExistingTrack(pTrack, entry, plan.preparationMode);
    if (outcome.changed) {
        ++pResult->existingTracksUpdated;
    }
    if (outcome.lockedGridKept) {
        pResult->lockedGridTracks.append(label);
    }
    pResult->droppedCues += outcome.droppedCues;
}
} // namespace

void updateNmlPlanComparisons(
        NmlImportPlan* pPlan, TrackCollectionManager* pTrackCollectionManager) {
    for (NmlPlanTrack& item : pPlan->tracks) {
        item.existingTrack = false;
        item.comparison = NmlPreparationComparison();
        if (item.entryIndex < 0 || item.entryIndex >= pPlan->document.entries.size()) {
            continue;
        }
        TrackPointer pTrack;
        if (item.status == NmlTrackStatus::MatchedInLibrary) {
            if (item.accepted) {
                pTrack = pTrackCollectionManager->getTrackById(item.libraryTrackId);
            }
        } else if (item.status != NmlTrackStatus::NotFound && !item.resolvedPath.isEmpty()) {
            pTrack = pTrackCollectionManager->getTrackByRef(
                    TrackRef::fromFilePath(item.resolvedPath));
        }
        if (!pTrack) {
            continue;
        }
        item.existingTrack = true;
        item.comparison = compareNmlPreparation(
                *pTrack, pPlan->document.entries.at(item.entryIndex));
    }
}

bool nmlPlanHasExistingPreparation(const NmlImportPlan& plan) {
    for (const NmlPlanTrack& item : plan.tracks) {
        if (item.existingTrack && item.comparison.trackHasPreparation()) {
            return true;
        }
    }
    return false;
}

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
    if (preparation >= 0) {
        others.append(preparation); // last, so the first list is never Preparation
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
        // Preparation is offered, but only ticked when it is the file's one list.
        choice.selected = importable.size() == 1 || playlist.name != QLatin1String("Preparation");
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
    QSet<TrackId> counted; // a track repeated in the playlist is counted and prepared once
    for (const NmlPlanTrack& item : plan.tracks) {
        const QString label = nmlPlanTrackLabel(item);
        // DJNote: an accepted library match puts that track in the Playlist;
        // an unticked one is applied like a missing row. The existing track's
        // preparation is the keep/replace rule's business, not touched here.
        if (item.status == NmlTrackStatus::MatchedInLibrary && item.accepted &&
                pTrackCollectionManager->getTrackById(item.libraryTrackId)) {
            ids.append(item.libraryTrackId);
            if (!counted.contains(item.libraryTrackId)) {
                counted.insert(item.libraryTrackId);
                // DJNote: the keep/replace rule for the existing track.
                applyKeepReplace(pTrackCollectionManager->getTrackById(item.libraryTrackId).get(),
                        plan, item, label, &result);
                ++result.matchedLibraryTracks;
                result.matchedLibraryLabels.append(label);
            }
            continue;
        }
        // DJNote: a copy identical to a library track is that track.
        if (item.reuseTrackId.isValid() && item.reuseAccepted &&
                pTrackCollectionManager->getTrackById(item.reuseTrackId)) {
            ids.append(item.reuseTrackId);
            if (!counted.contains(item.reuseTrackId)) {
                counted.insert(item.reuseTrackId);
                result.reusedTrackLabels.append(label);
            }
            continue;
        }
        const bool keptSeparate = item.keptSeparate || item.reuseTrackId.isValid();
        if (item.status == NmlTrackStatus::NotFound ||
                item.status == NmlTrackStatus::MatchedInLibrary) {
            TrackId missingId;
            if (ensureNmlMissingTrack(pTrackCollectionManager, item, &missingId)) {
                if (!counted.contains(missingId)) {
                    counted.insert(missingId);
                    result.missingTracks.append(label);
                    if (keptSeparate) {
                        result.keptSeparateLabels.append(label);
                    }
                }
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
            // library only; existing ones follow the keep/replace rule.
            if (item.entryIndex >= 0) {
                const NmlEntry& entry = plan.document.entries.at(item.entryIndex);
                applyNmlPreparation(pTrack.get(), entry);
                // DJNote: cues, colour, rating (one step per part).
                result.droppedCues += applyNmlCues(pTrack.get(), entry);
                applyNmlColorAndRating(pTrack.get(), entry);
            }
        }
        if (!counted.contains(pTrack->getId())) {
            counted.insert(pTrack->getId());
            if (keptSeparate) {
                result.keptSeparateLabels.append(label);
            }
            if (alreadyInLibrary) {
                // DJNote: the keep/replace rule for the existing track.
                applyKeepReplace(pTrack.get(), plan, item, label, &result);
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
    // DJNote: remember the source NML so the Playlist can be re-read.
    if (plan.playlistIndex >= 0 && plan.playlistIndex < plan.document.playlists.size()) {
        SourceNml sourceNml;
        sourceNml.playlistId = playlistId;
        sourceNml.nmlPath = plan.nmlPath;
        sourceNml.uuid = plan.document.playlists.at(plan.playlistIndex).uuid;
        sourceNml.name = plan.document.playlists.at(plan.playlistIndex).name;
        saveSourceNml(nmlDatabase(pTrackCollectionManager), sourceNml);
    }
    return result;
}

} // namespace nml
} // namespace mixxx
