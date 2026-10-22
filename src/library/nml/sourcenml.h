#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <optional>

class PlaylistDAO;

// DJNote: the Source NML of a Playlist created by an NML import.
// Lives in the DJNote-owned table `djnote_source_nml` of mixxx.db, created on
// startup when missing (never an upstream schema revision). Keyed by the
// Playlist id, so renaming keeps it and a duplicate (new id) has none.

namespace mixxx {
namespace nml {

struct SourceNml {
    int playlistId = -1;
    QString nmlPath;
    QString uuid; // the playlist's UUID inside the .nml (may be empty)
    QString name; // its name in the .nml
};

void ensureSourceNmlTable(const QSqlDatabase& database);

/// Inserts or replaces the Source NML of sourceNml.playlistId.
bool saveSourceNml(const QSqlDatabase& database, const SourceNml& sourceNml);

std::optional<SourceNml> loadSourceNml(const QSqlDatabase& database, int playlistId);

/// Removes the Source NML rows whose Playlist no longer exists. Playlist ids can be
/// reused by SQLite, and bulk deletes signal no id, so this is the cleanup.
void purgeOrphanSourceNml(const QSqlDatabase& database);

/// Creates the table, purges orphans, and purges again whenever a Playlist is deleted.
class SourceNmlKeeper : public QObject {
    Q_OBJECT
  public:
    SourceNmlKeeper(PlaylistDAO* pPlaylistDao, const QSqlDatabase& database, QObject* parent);

  private:
    QSqlDatabase m_database;
};

} // namespace nml
} // namespace mixxx
