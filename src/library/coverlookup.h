#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>

#include "library/coverart.h"
#include "library/coverartutils.h"
#include "track/trackid.h"

class TrackCollection;

/// DJNote: answers the cover of a track by its location, for track lists
/// whose rows are files rather than library tracks (Computer, Recording,
/// later the external libraries).
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

  signals:
    /// The answer for `location` was determined or has changed: ask again.
    void coverInfoChanged(const QString& location);

  private slots:
    void slotTracksChanged(const QSet<TrackId>& trackIds);
    void slotTracksAdded(const QSet<TrackId>& trackIds);

  private:
    struct Answer {
        CoverInfo coverInfo;
        // Valid if the answer is the library's cover.
        TrackId trackId;
    };

    bool lookUpLibraryCover(const QString& location, Answer* pAnswer);
    void readInBackground(const QString& location, const Answer& libraryAnswer);
    void slotCoverRead(const QString& location, const Answer& answer);
    void forget(const QString& location);

    TrackCollection* const m_pTrackCollection;
    QHash<QString, Answer> m_answers;
    // The locations of the answers (and reads) that are library covers.
    QHash<TrackId, QString> m_libraryLocations;
    // Library covers answered from track objects in memory.
    QHash<QString, QMetaObject::Connection> m_trackConnections;
    // Locations being read, with the track id if reading a library cover.
    QHash<QString, TrackId> m_reading;
    // Only used on the reader thread. Remembers the image files of the last
    // folder read.
    CoverInfoGuesser m_guesser;
    // A single reader thread, so files of a folder are read one after the
    // other and the folder's image files are listed only once.
    QThreadPool m_readerPool;
};
