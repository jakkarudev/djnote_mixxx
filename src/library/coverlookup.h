#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <atomic>
#include <memory>

#include "library/coverart.h"
#include "library/coverartutils.h"
#include "track/trackid.h"

class CoverBatch;
class TrackCollection;

/// DJNote: answers the cover of a track by its location, for track lists
/// whose rows are files rather than library tracks (Computer, Recording and
/// the external libraries).
///
/// Showing a cover never adds a track to the library:
///  - A track in the library answers the library's cover (which the DJ may
///    have customized), read without creating or adding a track.
///  - Any other file answers what Mixxx's import would guess (embedded cover
///    first, then an image in the track's folder). The file is read on a
///    worker thread and nothing is saved.
///  - A missing file answers an empty cover.
///
/// Answers are remembered for the session. An answer that has an image always
/// has an image digest: the cover column's delegate would otherwise resolve
/// (and so add) a track by location.
class CoverLookup : public QObject {
    Q_OBJECT
  public:
    explicit CoverLookup(
            TrackCollection* pTrackCollection,
            QObject* pParent = nullptr);
    ~CoverLookup() override;

    /// Returns the cover for the track at `location` right away. A file that
    /// is not in the library and was not read yet answers an empty cover
    /// while it is read in the background; coverInfoChanged() follows.
    CoverInfo coverInfo(const QString& location);

    /// Reads the covers of `locations` that are not known yet, in the
    /// background, e.g. to sort a list by cover. Once the batch has
    /// finished, coverInfo() answers every location right away. A batch
    /// whose covers are all known is finished when returned. Cancelling or
    /// destroying the batch stops reading the covers nothing else waits for.
    std::unique_ptr<CoverBatch> readCovers(const QStringList& locations);

  signals:
    /// The answer for `location` was determined or has changed: ask again.
    void coverInfoChanged(const QString& location);

  private slots:
    void slotTracksChanged(const QSet<TrackId>& trackIds);
    void slotTracksAdded(const QSet<TrackId>& trackIds);

  private:
    friend class CoverBatch;

    struct Answer {
        CoverInfo coverInfo;
        // Valid if the answer is the library's cover.
        TrackId trackId;
    };

    struct Reading {
        // Tells a result from a read started later for the same location.
        quint64 id;
        // Valid if reading a library cover.
        TrackId trackId;
        // Set when nothing waits for the read any more: skip it.
        std::shared_ptr<std::atomic<bool>> pDropped;
        // The batches waiting for the read.
        int batchCount = 0;
        // Asked by coverInfo(), e.g. for a cell being drawn.
        bool asked = false;
    };

    /// Answers `location` from the library right away if it can, else starts
    /// reading it. Returns whether it is answered.
    bool answerOrRead(const QString& location, int priority);
    bool lookUpLibraryCover(const QString& location, Answer* pAnswer);
    void readInBackground(
            const QString& location,
            const Answer& libraryAnswer,
            int priority);
    void slotCoverRead(const QString& location, quint64 readId, const Answer& answer);
    void forget(const QString& location);
    /// Counts `location` as done for the batches waiting for it.
    void answeredForBatches(const QString& location);
    void releaseBatch(CoverBatch* pBatch);

    TrackCollection* const m_pTrackCollection;
    QHash<QString, Answer> m_answers;
    // The locations of the answers (and reads) that are library covers.
    QHash<TrackId, QString> m_libraryLocations;
    // Library covers answered from track objects in memory.
    QHash<QString, QMetaObject::Connection> m_trackConnections;
    // Locations being read.
    QHash<QString, Reading> m_reading;
    quint64 m_nextReadId = 0;
    // The batches not finished yet.
    QList<CoverBatch*> m_batches;
    // Only used on the reader thread. Remembers the image files of the last
    // folder read.
    CoverInfoGuesser m_guesser;
    // A single reader thread, so files of a folder are read one after the
    // other and the folder's image files are listed only once.
    QThreadPool m_readerPool;
};

/// DJNote: a batch of covers read by the cover lookup, see
/// CoverLookup::readCovers().
class CoverBatch : public QObject {
    Q_OBJECT
  public:
    /// Cancels the batch if it has not finished.
    ~CoverBatch() override;

    /// The number of distinct locations of the batch.
    int total() const {
        return m_total;
    }
    /// The number of them whose cover is known.
    int done() const {
        return m_total - static_cast<int>(m_pending.size());
    }
    bool isFinished() const {
        return !m_cancelled && m_pending.isEmpty();
    }
    bool isCancelled() const {
        return m_cancelled;
    }

    /// Stops reading the covers of the batch that nothing else waits for.
    /// finished() is not emitted.
    void cancel();

  signals:
    void progress(int done, int total);
    /// Every cover of the batch is known.
    void finished();

  private:
    friend class CoverLookup;
    explicit CoverBatch(CoverLookup* pLookup);

    const QPointer<CoverLookup> m_pLookup;
    QSet<QString> m_pending;
    int m_total = 0;
    bool m_cancelled = false;
};
