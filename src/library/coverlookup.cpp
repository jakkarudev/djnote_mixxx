#include "library/coverlookup.h"

#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlRecord>

#include "library/coverartutils.h"
#include "library/dao/trackschema.h"
#include "library/queryutil.h"
#include "library/trackcollection.h"
#include "moc_coverlookup.cpp"
#include "sources/soundsourceproxy.h"
#include "track/globaltrackcache.h"
#include "track/track.h"

namespace {

/// What Mixxx's import would pick as the cover of a file: its embedded cover
/// first, then an image in its folder. Runs on the reader thread.
CoverInfo guessCoverOfFile(const QString& location, CoverInfoGuesser* pGuesser) {
    const mixxx::FileInfo fileInfo(location);
    if (!fileInfo.checkFileExists()) {
        return CoverInfo();
    }
    mixxx::TrackMetadata trackMetadata;
    QImage embeddedCover;
    constexpr bool kResetMissingTagMetadata = false;
    SoundSourceProxy::importTrackMetadataAndCoverImageFromFile(
            mixxx::FileAccess(fileInfo),
            &trackMetadata,
            &embeddedCover,
            kResetMissingTagMetadata);
    return CoverInfo(
            pGuesser->guessCoverInfo(
                    fileInfo,
                    trackMetadata.getAlbumInfo().getTitle(),
                    embeddedCover),
            location);
}

/// A library cover with only a legacy hash, completed with the digest of its
/// image. Runs on the reader thread.
CoverInfo withImageDigest(const CoverInfo& libraryCover) {
    const QImage image = libraryCover.loadImage().image;
    if (image.isNull()) {
        return CoverInfo();
    }
    CoverInfo coverInfo = libraryCover;
    coverInfo.setImageDigest(image);
    return coverInfo;
}

// Cells being drawn are read before the covers of a batch.
constexpr int kAskedPriority = 1;
constexpr int kBatchPriority = 0;

} // namespace

CoverLookup::CoverLookup(
        TrackCollection* pTrackCollection,
        QObject* pParent)
        : QObject(pParent),
          m_pTrackCollection(pTrackCollection) {
    m_readerPool.setMaxThreadCount(1);
    // Changed and removed tracks: their library cover is no longer valid.
    connect(m_pTrackCollection,
            &TrackCollection::tracksChanged,
            this,
            &CoverLookup::slotTracksChanged);
    connect(m_pTrackCollection,
            &TrackCollection::tracksRemoved,
            this,
            &CoverLookup::slotTracksChanged);
    // Added tracks: a file's guessed cover is replaced by the library's.
    connect(m_pTrackCollection,
            &TrackCollection::tracksAdded,
            this,
            &CoverLookup::slotTracksAdded);
}

CoverLookup::~CoverLookup() {
    m_readerPool.clear();
    m_readerPool.waitForDone();
}

CoverInfo CoverLookup::coverInfo(const QString& location) {
    if (location.isEmpty()) {
        return CoverInfo();
    }
    const auto it = m_answers.constFind(location);
    if (it != m_answers.constEnd()) {
        return it->coverInfo;
    }
    if (!m_reading.contains(location) && answerOrRead(location, kAskedPriority)) {
        return m_answers.value(location).coverInfo;
    }
    m_reading[location].asked = true;
    return CoverInfo();
}

std::unique_ptr<CoverBatch> CoverLookup::readCovers(const QStringList& locations) {
    std::unique_ptr<CoverBatch> pBatch(new CoverBatch(this));
    QSet<QString> distinct;
    for (const auto& location : locations) {
        if (distinct.contains(location)) {
            continue;
        }
        distinct.insert(location);
        if (location.isEmpty() || m_answers.contains(location)) {
            continue;
        }
        if (!m_reading.contains(location) && answerOrRead(location, kBatchPriority)) {
            continue;
        }
        ++m_reading[location].batchCount;
        pBatch->m_pending.insert(location);
    }
    pBatch->m_total = static_cast<int>(distinct.size());
    if (!pBatch->m_pending.isEmpty()) {
        m_batches.append(pBatch.get());
    }
    return pBatch;
}

bool CoverLookup::answerOrRead(const QString& location, int priority) {
    DEBUG_ASSERT(!m_answers.contains(location));
    DEBUG_ASSERT(!m_reading.contains(location));
    Answer answer;
    if (lookUpLibraryCover(location, &answer)) {
        m_libraryLocations.insert(answer.trackId, location);
        if (!QFileInfo::exists(location)) {
            answer.coverInfo = CoverInfo();
        }
        if (answer.coverInfo.hasImage() && answer.coverInfo.imageDigest().isEmpty()) {
            // Saved by an old Mixxx version with only a legacy hash.
            readInBackground(location, answer, priority);
            return false;
        }
        m_answers.insert(location, answer);
        return true;
    }
    readInBackground(location, Answer(), priority);
    return false;
}

void CoverLookup::slotTracksChanged(const QSet<TrackId>& trackIds) {
    for (const auto& trackId : trackIds) {
        const QString location = m_libraryLocations.value(trackId);
        if (!location.isEmpty()) {
            forget(location);
        }
    }
}

void CoverLookup::slotTracksAdded(const QSet<TrackId>& trackIds) {
    // Every library cover is either answered or being read.
    if (m_answers.size() + m_reading.size() == m_libraryLocations.size()) {
        // Only library covers are known, no file cover to replace.
        return;
    }
    for (const auto& trackId : trackIds) {
        const TrackPointer pCachedTrack = GlobalTrackCacheLocker().lookupTrackById(trackId);
        const QString location = pCachedTrack
                ? pCachedTrack->getLocation()
                : m_pTrackCollection->getTrackDAO().getTrackLocation(trackId);
        if (m_answers.contains(location) || m_reading.contains(location)) {
            forget(location);
        }
    }
}

bool CoverLookup::lookUpLibraryCover(const QString& location, Answer* pAnswer) {
    QSqlQuery query(m_pTrackCollection->database());
    query.prepare(QStringLiteral(
            "SELECT " LIBRARY_TABLE ".%1," LIBRARY_TABLE ".%2," LIBRARY_TABLE
            ".%3," LIBRARY_TABLE ".%4," LIBRARY_TABLE ".%5," LIBRARY_TABLE
            ".%6," LIBRARY_TABLE ".%7 "
            "FROM " LIBRARY_TABLE " INNER JOIN " TRACKLOCATIONS_TABLE
            " ON " LIBRARY_TABLE ".%8=" TRACKLOCATIONS_TABLE ".%10 "
            "WHERE " TRACKLOCATIONS_TABLE ".%9=:location")
                    .arg(LIBRARYTABLE_ID,
                            LIBRARYTABLE_COVERART_SOURCE,
                            LIBRARYTABLE_COVERART_TYPE,
                            LIBRARYTABLE_COVERART_LOCATION,
                            LIBRARYTABLE_COVERART_COLOR,
                            LIBRARYTABLE_COVERART_DIGEST,
                            LIBRARYTABLE_COVERART_HASH,
                            LIBRARYTABLE_LOCATION,
                            TRACKLOCATIONSTABLE_LOCATION,
                            TRACKLOCATIONSTABLE_ID));
    query.bindValue(QStringLiteral(":location"), location);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return false;
    }
    if (!query.next()) {
        return false;
    }
    pAnswer->trackId = TrackId(query.value(0));

    // A track loaded in memory may carry changes not saved yet.
    const TrackPointer pCachedTrack =
            GlobalTrackCacheLocker().lookupTrackById(pAnswer->trackId);
    if (pCachedTrack) {
        pAnswer->coverInfo = pCachedTrack->getCoverInfoWithLocation();
        // The library only relays changes of tracks it loaded itself, not of
        // tracks it just added (e.g. a file selected in Computer), so watch
        // the track object directly.
        m_trackConnections.insert(location,
                connect(pCachedTrack.get(),
                        &Track::coverArtUpdated,
                        this,
                        [this, location] {
                            forget(location);
                        }));
        return true;
    }

    CoverInfo coverInfo;
    coverInfo.setImageDigest(
            query.value(5).toByteArray(),
            static_cast<quint16>(query.value(6).toUInt()));
    coverInfo.color = mixxx::RgbColor::fromQVariant(query.value(4));
    if (coverInfo.hasCacheKey()) {
        coverInfo.type = static_cast<CoverInfo::Type>(query.value(2).toInt());
        coverInfo.source = static_cast<CoverInfo::Source>(query.value(1).toInt());
        coverInfo.coverLocation = query.value(3).toString();
    }
    coverInfo.trackLocation = location;
    pAnswer->coverInfo = coverInfo;
    return true;
}

void CoverLookup::readInBackground(
        const QString& location,
        const Answer& libraryAnswer,
        int priority) {
    DEBUG_ASSERT(!m_reading.contains(location));
    Reading reading;
    reading.id = m_nextReadId++;
    reading.trackId = libraryAnswer.trackId;
    reading.pDropped = std::make_shared<std::atomic<bool>>(false);
    m_reading.insert(location, reading);
    m_readerPool.start(
            [this,
                    location,
                    libraryAnswer,
                    readId = reading.id,
                    pDropped = reading.pDropped] {
                if (pDropped->load()) {
                    return;
                }
                Answer answer = libraryAnswer;
                if (answer.trackId.isValid()) {
                    answer.coverInfo = withImageDigest(answer.coverInfo);
                } else {
                    answer.coverInfo = guessCoverOfFile(location, &m_guesser);
                }
                QMetaObject::invokeMethod(
                        this,
                        [this, location, readId, answer] {
                            slotCoverRead(location, readId, answer);
                        },
                        Qt::QueuedConnection);
            },
            priority);
}

void CoverLookup::slotCoverRead(const QString& location, quint64 readId, const Answer& answer) {
    const auto it = m_reading.constFind(location);
    if (it == m_reading.constEnd() || it->id != readId) {
        // Forgotten or dropped while being read.
        return;
    }
    m_reading.erase(it);
    m_answers.insert(location, answer);
    emit coverInfoChanged(location);
    answeredForBatches(location);
}

void CoverLookup::forget(const QString& location) {
    const auto answerIt = m_answers.constFind(location);
    if (answerIt != m_answers.constEnd()) {
        m_libraryLocations.remove(answerIt->trackId);
        m_answers.erase(answerIt);
    }
    int batchCount = 0;
    const auto readingIt = m_reading.constFind(location);
    if (readingIt != m_reading.constEnd()) {
        // The read in progress is dropped when it arrives.
        batchCount = readingIt->batchCount;
        readingIt->pDropped->store(true);
        m_libraryLocations.remove(readingIt->trackId);
        m_reading.erase(readingIt);
    }
    disconnect(m_trackConnections.take(location));
    if (batchCount > 0) {
        // The batches waiting for it wait for the new answer.
        if (answerOrRead(location, kBatchPriority)) {
            answeredForBatches(location);
        } else {
            m_reading[location].batchCount = batchCount;
        }
    }
    emit coverInfoChanged(location);
}

void CoverLookup::answeredForBatches(const QString& location) {
    // A slot may cancel or destroy any batch.
    const QList<CoverBatch*> batches = m_batches;
    for (CoverBatch* pBatch : batches) {
        if (!m_batches.contains(pBatch) || !pBatch->m_pending.remove(location)) {
            continue;
        }
        const bool finished = pBatch->m_pending.isEmpty();
        if (finished) {
            m_batches.removeOne(pBatch);
        }
        const QPointer<CoverBatch> pGuard(pBatch);
        emit pBatch->progress(pBatch->done(), pBatch->total());
        if (finished && pGuard) {
            emit pBatch->finished();
        }
    }
}

void CoverLookup::releaseBatch(CoverBatch* pBatch) {
    if (!m_batches.removeOne(pBatch)) {
        return;
    }
    for (const auto& location : std::as_const(pBatch->m_pending)) {
        const auto it = m_reading.find(location);
        if (it == m_reading.end()) {
            continue;
        }
        if (--it->batchCount > 0 || it->asked) {
            continue;
        }
        // Nothing waits for it any more.
        it->pDropped->store(true);
        m_libraryLocations.remove(it->trackId);
        m_reading.erase(it);
    }
}

CoverBatch::CoverBatch(CoverLookup* pLookup)
        : m_pLookup(pLookup) {
}

CoverBatch::~CoverBatch() {
    cancel();
}

void CoverBatch::cancel() {
    if (m_cancelled || isFinished()) {
        return;
    }
    m_cancelled = true;
    if (m_pLookup) {
        m_pLookup->releaseBatch(this);
    }
}
