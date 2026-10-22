#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <optional>

class PlaylistDAO;

// DJNote (#114): the "source NML" of a Playlist created by an NML import.
// Lives in the DJNote-owned table `djnote_nml_source` of mixxx.db, created on
// startup when missing (never an upstream schema revision). Keyed by the
// Playlist id, so renaming keeps the link and a duplicate (new id) has none.

namespace mixxx {
namespace nml {

struct NmlSourceLink {
    int playlistId = -1;
    QString nmlPath;
    QString uuid; // the Traktor playlist's UUID inside the .nml (may be empty)
    QString name; // its name in the .nml
};

void ensureNmlSourceTable(const QSqlDatabase& database);

/// Inserts or replaces the link of link.playlistId.
bool saveNmlSourceLink(const QSqlDatabase& database, const NmlSourceLink& link);

std::optional<NmlSourceLink> loadNmlSourceLink(const QSqlDatabase& database, int playlistId);

/// Removes the links whose Playlist no longer exists. Playlist ids can be
/// reused by SQLite, and bulk deletes signal no id, so this is the cleanup.
void purgeOrphanNmlSourceLinks(const QSqlDatabase& database);

/// Creates the table, purges orphans, and purges again whenever a Playlist is deleted.
class NmlSourceLinkKeeper : public QObject {
    Q_OBJECT
  public:
    NmlSourceLinkKeeper(PlaylistDAO* pPlaylistDao, const QSqlDatabase& database, QObject* parent);

  private:
    QSqlDatabase m_database;
};

} // namespace nml
} // namespace mixxx
