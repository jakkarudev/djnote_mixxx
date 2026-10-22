#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QThread>
#include <atomic>

#include "library/nml/nmlwriter.h"

class Track;
class TrackCollectionManager;

// DJNote: GUI-free NML export.
//
//   nmlExportItemsForPlaylist - Playlist -> NmlWriteEntry list (tags and
//                               preparation), playlist order, repeats kept.
//   NmlExportWorker           - copies the tracks flat next to the .nml (optional),
//                               then writes the .nml LAST; no .nml after a cancel.
//
// The worker is a QThread like TrackExportWorker: start() it, or call run()
// directly in tests; read result() after it has finished.

namespace mixxx {
namespace nml {

struct NmlExportJob {
    // Full path of the .nml to write. Its folder is the export folder.
    QString nmlPath;
    QString playlistName;
    bool copyTracks = true;
    // Playlist order, repeats kept. NmlWriteEntry::path is the ORIGINAL file.
    QList<NmlWriteEntry> items;
    QDateTime exportTimeUtc = QDateTime::currentDateTimeUtc();
};

struct NmlExportResult {
    QString error;          // non-empty: nothing usable was written
    bool canceled = false;  // no .nml was written
    QString nmlPath;
    int tracksInPlaylist = 0; // playlist items in the .nml
    int copied = 0;           // files newly copied
    int alreadyThere = 0;     // same name and size already in the folder
    int renamed = 0;          // clash: copied under a numbered name
    QStringList missing;      // one line per playlist item skipped (file gone)
    QStringList failed;       // one line per file that could not be copied
};

/// "<lastFolder>/<Playlist name>/<Playlist name>.nml" with the name made
/// safe as a file name. Empty lastFolder: the Music folder.
QString nmlDefaultExportPath(const QString& lastFolder, const QString& playlistName);

/// The folder to remember after the DJ chose `nmlPath`: the parent of the
/// Playlist folder when the .nml sits in a folder named like the Playlist,
/// else the folder of the .nml.
QString nmlFolderToRemember(const QString& nmlPath, const QString& playlistName);

/// Name of a clash copy: "01 - Intro.flac", n=1 -> "01 - Intro 1.flac".
QString nmlNumberedFileName(const QString& fileName, int n);

/// Human-readable summary of an export.
QString nmlExportSummary(const NmlExportResult& result);

/// The one place that turns a Track into an NmlWriteEntry (tags and preparation).
NmlWriteEntry nmlWriteEntryFromTrack(const Track& track);

/// Playlist `playlistId` as write entries (playlist order, repeats kept).
/// Each entry carries the preparation (grid, cues, key, colour, rating,
/// lock): see nmlWriteEntryFromTrack().
QList<NmlWriteEntry> nmlExportItemsForPlaylist(
        TrackCollectionManager* pTrackCollectionManager, int playlistId);

class NmlExportWorker : public QThread {
    Q_OBJECT
  public:
    explicit NmlExportWorker(const NmlExportJob& job, QObject* parent = nullptr);

    void run() override;

    /// Thread-safe. Stops after the file being copied.
    void stop();

    const NmlExportResult& result() const {
        return m_result;
    }

  signals:
    /// Before each file: its name, 1-based index, number of files.
    void progress(const QString& fileName, int index, int count);
    void canceled();

  private:
    NmlExportJob m_job;
    NmlExportResult m_result;
    std::atomic<bool> m_stop{false};
};

} // namespace nml
} // namespace mixxx
