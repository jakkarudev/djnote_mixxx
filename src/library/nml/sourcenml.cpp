#include "library/nml/nmlsourcelink.h"

#include <QSqlQuery>

#include "library/dao/playlistdao.h"

namespace mixxx {
namespace nml {

void ensureNmlSourceTable(const QSqlDatabase& database) {
    QSqlQuery query(database);
    query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS djnote_nml_source ("
            "playlist_id INTEGER PRIMARY KEY, "
            "nml_path TEXT, uuid TEXT, name TEXT)"));
}

bool saveNmlSourceLink(const QSqlDatabase& database, const NmlSourceLink& link) {
    ensureNmlSourceTable(database);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO djnote_nml_source (playlist_id, nml_path, uuid, name) "
            "VALUES (:id, :path, :uuid, :name)"));
    query.bindValue(QStringLiteral(":id"), link.playlistId);
    query.bindValue(QStringLiteral(":path"), link.nmlPath);
    query.bindValue(QStringLiteral(":uuid"), link.uuid);
    query.bindValue(QStringLiteral(":name"), link.name);
    return query.exec();
}

std::optional<NmlSourceLink> loadNmlSourceLink(const QSqlDatabase& database, int playlistId) {
    ensureNmlSourceTable(database);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
            "SELECT nml_path, uuid, name FROM djnote_nml_source WHERE playlist_id=:id"));
    query.bindValue(QStringLiteral(":id"), playlistId);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    NmlSourceLink link;
    link.playlistId = playlistId;
    link.nmlPath = query.value(0).toString();
    link.uuid = query.value(1).toString();
    link.name = query.value(2).toString();
    return link;
}

void purgeOrphanNmlSourceLinks(const QSqlDatabase& database) {
    ensureNmlSourceTable(database);
    QSqlQuery query(database);
    query.exec(QStringLiteral(
            "DELETE FROM djnote_nml_source WHERE playlist_id NOT IN (SELECT id FROM Playlists)"));
}

NmlSourceLinkKeeper::NmlSourceLinkKeeper(
        PlaylistDAO* pPlaylistDao, const QSqlDatabase& database, QObject* parent)
        : QObject(parent),
          m_database(database) {
    purgeOrphanNmlSourceLinks(m_database);
    connect(pPlaylistDao,
            &PlaylistDAO::deleted,
            this,
            [this](int) { purgeOrphanNmlSourceLinks(m_database); });
}

} // namespace nml
} // namespace mixxx

#include "moc_nmlsourcelink.cpp"
