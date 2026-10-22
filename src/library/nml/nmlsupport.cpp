#include "library/nml/nmlsupport.h"

#include <QSqlQuery>

#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "track/track.h"
#include "track/trackref.h"

namespace mixxx {
namespace nml {

QSqlDatabase nmlDatabase(TrackCollectionManager* pManager) {
    return pManager->internalCollection()->database();
}

QString nmlPlanTrackLabel(const NmlPlanTrack& item) {
    if (item.title.isEmpty()) {
        return item.primaryKey;
    }
    return item.artist.isEmpty() ? item.title
                                 : item.artist + QStringLiteral(" - ") + item.title;
}

bool ensureNmlMissingTrack(
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
    }
    *pId = pTrack->getId();
    QSqlQuery query(nmlDatabase(pManager));
    query.prepare(QStringLiteral(
            "UPDATE track_locations SET fs_deleted=1 WHERE id="
            "(SELECT location FROM library WHERE id=:id)"));
    query.bindValue(QStringLiteral(":id"), pTrack->getId().toVariant());
    return query.exec();
}

} // namespace nml
} // namespace mixxx
