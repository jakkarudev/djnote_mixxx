#include "library/nml/nmlexport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QSqlQuery>
#include <QStandardPaths>

#include "library/nml/nmlexportpreparation.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "track/track.h"
#include "track/trackid.h"

namespace mixxx {
namespace nml {

namespace {

QString tr(const char* text) {
    return QCoreApplication::translate("NmlExport", text);
}

QString safeFileName(const QString& name) {
    QString out = name;
    for (QChar& c : out) {
        if (QStringLiteral("<>:\"/\\|?*").contains(c) || c.unicode() < 32) {
            c = QLatin1Char('_');
        }
    }
    out = out.trimmed();
    while (out.endsWith(QLatin1Char('.'))) {
        out.chop(1);
    }
    return out.isEmpty() ? QStringLiteral("Playlist") : out;
}

QString itemLabel(const NmlWriteEntry& item) {
    if (item.title.isEmpty()) {
        return QFileInfo(item.path).fileName();
    }
    return item.artist.isEmpty() ? item.title : item.artist + QStringLiteral(" - ") + item.title;
}

bool sameNameAndSize(const QString& a, const QString& b) {
    return QFileInfo(a).size() == QFileInfo(b).size();
}

} // namespace

QString nmlDefaultExportPath(const QString& lastFolder, const QString& playlistName) {
    const QString base = lastFolder.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
            : lastFolder;
    const QString name = safeFileName(playlistName);
    return QDir(base).filePath(name + QLatin1Char('/') + name + QStringLiteral(".nml"));
}

QString nmlFolderToRemember(const QString& nmlPath, const QString& playlistName) {
    const QFileInfo info(nmlPath);
    const QString dir = info.absolutePath();
    const QString dirName = dir.mid(dir.lastIndexOf(QLatin1Char('/')) + 1);
    if (dirName == safeFileName(playlistName)) {
        const QString parent = QFileInfo(dir).path(); // string-based: the folder may not exist yet
        if (parent != dir) {
            return parent;
        }
    }
    return dir;
}

QString nmlNumberedFileName(const QString& fileName, int n) {
    if (n <= 0) {
        return fileName;
    }
    const QFileInfo info(fileName);
    const QString suffix = info.suffix();
    const QString base = info.completeBaseName() + QLatin1Char(' ') + QString::number(n);
    return suffix.isEmpty() ? base : base + QLatin1Char('.') + suffix;
}

QString nmlExportSummary(const NmlExportResult& result) {
    if (!result.error.isEmpty()) {
        return result.error;
    }
    if (result.canceled) {
        return tr("Export canceled. No .nml file was written.");
    }
    QString text = tr("Exported %1 tracks to \"%2\".")
                           .arg(QString::number(result.tracksInPlaylist), result.nmlPath);
    if (result.copied + result.alreadyThere + result.renamed > 0) {
        text += QStringLiteral("\n") +
                tr("%1 files copied, %2 already there, %3 renamed because of a name clash.")
                        .arg(QString::number(result.copied),
                                QString::number(result.alreadyThere),
                                QString::number(result.renamed));
    }
    if (!result.missing.isEmpty()) {
        text += QStringLiteral("\n\n") +
                tr("%1 tracks are missing and were skipped:").arg(QString::number(result.missing.size())) +
                QStringLiteral("\n") + result.missing.join(QLatin1Char('\n'));
    }
    if (!result.failed.isEmpty()) {
        text += QStringLiteral("\n\n") +
                tr("%1 files could not be copied and were skipped:").arg(QString::number(result.failed.size())) +
                QStringLiteral("\n") + result.failed.join(QLatin1Char('\n'));
    }
    return text;
}

namespace {
// #116: this is the one place that turns a Track into an NmlWriteEntry. Add
// the preparation here (bpm/cues/key/colour/rating/locked); the writer and
// the worker need no change.
NmlWriteEntry nmlWriteEntryFromTrack(const Track& track) {
    NmlWriteEntry entry;
    entry.path = track.getLocation();
    entry.title = track.getTitle();
    entry.artist = track.getArtist();
    entry.album = track.getAlbum();
    entry.trackNumber = track.getTrackNumber();
    entry.genre = track.getGenre();
    entry.comment = track.getComment();
    entry.releaseDate = track.getYear();
    entry.playtimeSeconds = track.getDuration();
    entry.bitrateBitsPerSecond = track.getBitrate() * 1000;
    fillNmlPreparation(&entry, track); // DJNote #116
    return entry;
}
} // namespace

QList<NmlWriteEntry> nmlExportItemsForPlaylist(
        TrackCollectionManager* pTrackCollectionManager, int playlistId) {
    QList<NmlWriteEntry> items;
    QSqlQuery query(pTrackCollectionManager->internalCollection()->database());
    query.prepare(QStringLiteral(
            "SELECT track_id FROM PlaylistTracks WHERE playlist_id=:id ORDER BY position"));
    query.bindValue(QStringLiteral(":id"), playlistId);
    if (!query.exec()) {
        return items;
    }
    while (query.next()) {
        const TrackId trackId(query.value(0));
        const TrackPointer pTrack = pTrackCollectionManager->getTrackById(trackId);
        if (pTrack) {
            items.append(nmlWriteEntryFromTrack(*pTrack));
        }
    }
    return items;
}

NmlExportWorker::NmlExportWorker(const NmlExportJob& job, QObject* parent)
        : QThread(parent), m_job(job) {
}

void NmlExportWorker::stop() {
    m_stop = true;
}

void NmlExportWorker::run() {
    m_result = NmlExportResult();
    m_result.nmlPath = m_job.nmlPath;

    const QFileInfo nmlInfo(m_job.nmlPath);
    const QString exportDir = nmlInfo.absolutePath();
    if (!QDir().mkpath(exportDir)) {
        m_result.error = tr("Could not create the folder \"%1\".").arg(exportDir);
        return;
    }

    // One collection entry per distinct file; the Playlist keeps the repeats.
    NmlWriteDocument doc;
    doc.playlistName = m_job.playlistName;
    doc.exportTimeUtc = m_job.exportTimeUtc;
    QHash<QString, int> entryIndexBySource;
    for (int i = 0; i < m_job.items.size(); ++i) {
        const NmlWriteEntry& item = m_job.items.at(i);
        const QFileInfo source(item.path);
        if (!source.isFile()) {
            m_result.missing.append(itemLabel(item));
            continue;
        }
        const QString sourceKey = source.canonicalFilePath();
        auto it = entryIndexBySource.constFind(sourceKey);
        if (it == entryIndexBySource.constEnd()) {
            it = entryIndexBySource.insert(sourceKey, doc.entries.size());
            doc.entries.append(item);
        }
        doc.playlistOrder.append(it.value());
    }

    if (m_job.copyTracks) {
        QSet<QString> claimed; // destination names (lower case) used in this run
        for (int e = 0; e < doc.entries.size(); ++e) {
            if (m_stop) {
                m_result.canceled = true;
                emit canceled();
                return;
            }
            NmlWriteEntry& entry = doc.entries[e];
            const QFileInfo source(entry.path);
            emit progress(source.fileName(), e + 1, doc.entries.size());

            // Already in the export folder: reference it where it is.
            if (source.absolutePath() == exportDir) {
                claimed.insert(source.fileName().toLower());
                continue;
            }

            QString destPath;
            bool reuse = false;
            bool numbered = false;
            for (int n = 0;; ++n) {
                const QString name = nmlNumberedFileName(source.fileName(), n);
                const QString candidate = QDir(exportDir).filePath(name);
                if (claimed.contains(name.toLower())) {
                    continue;
                }
                if (!QFileInfo::exists(candidate)) {
                    destPath = candidate;
                    numbered = n > 0;
                    break;
                }
                if (sameNameAndSize(candidate, source.absoluteFilePath())) {
                    destPath = candidate;
                    reuse = true;
                    break;
                }
            }
            claimed.insert(QFileInfo(destPath).fileName().toLower());
            if (reuse) {
                ++m_result.alreadyThere;
            } else if (QFile::copy(source.absoluteFilePath(), destPath)) {
                ++m_result.copied;
                if (numbered) {
                    ++m_result.renamed;
                }
            } else {
                m_result.failed.append(itemLabel(entry));
                // Drop this entry from the Playlist: mark with an empty path.
                entry.path.clear();
                continue;
            }
            entry.path = destPath;
        }
        if (m_stop) {
            m_result.canceled = true;
            emit canceled();
            return;
        }

        // Remove entries whose copy failed, remapping the Playlist order.
        QList<int> remap(doc.entries.size(), -1);
        QList<NmlWriteEntry> kept;
        for (int e = 0; e < doc.entries.size(); ++e) {
            if (!doc.entries.at(e).path.isEmpty()) {
                remap[e] = kept.size();
                kept.append(doc.entries.at(e));
            }
        }
        QList<int> order;
        for (const int e : std::as_const(doc.playlistOrder)) {
            if (remap.at(e) >= 0) {
                order.append(remap.at(e));
            }
        }
        doc.entries = kept;
        doc.playlistOrder = order;
    }

    // The .nml is written last.
    if (m_stop) {
        m_result.canceled = true;
        emit canceled();
        return;
    }
    QSaveFile file(m_job.nmlPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(writeNml(doc)) < 0 || !file.commit()) {
        m_result.error = tr("Could not write \"%1\".").arg(m_job.nmlPath);
        return;
    }
    m_result.tracksInPlaylist = doc.playlistOrder.size();
}

} // namespace nml
} // namespace mixxx

#include "moc_nmlexport.cpp"
