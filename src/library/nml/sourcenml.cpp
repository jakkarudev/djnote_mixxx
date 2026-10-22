#include "library/nml/sourcenml.h"

#include <QSqlQuery>

#include "library/dao/playlistdao.h"

namespace mixxx {
namespace nml {

void ensureSourceNmlTable(const QSqlDatabase& database) {
    QSqlQuery query(database);
    query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS djnote_source_nml ("
            "playlist_id INTEGER PRIMARY KEY, "
            "nml_path TEXT, uuid TEXT, name TEXT)"));
}

bool saveSourceNml(const QSqlDatabase& database, const SourceNml& sourceNml) {
    ensureSourceNmlTable(database);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO djnote_source_nml (playlist_id, nml_path, uuid, name) "
            "VALUES (:id, :path, :uuid, :name)"));
    query.bindValue(QStringLiteral(":id"), sourceNml.playlistId);
    query.bindValue(QStringLiteral(":path"), sourceNml.nmlPath);
    query.bindValue(QStringLiteral(":uuid"), sourceNml.uuid);
    query.bindValue(QStringLiteral(":name"), sourceNml.name);
    return query.exec();
}

std::optional<SourceNml> loadSourceNml(const QSqlDatabase& database, int playlistId) {
    ensureSourceNmlTable(database);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
            "SELECT nml_path, uuid, name FROM djnote_source_nml WHERE playlist_id=:id"));
    query.bindValue(QStringLiteral(":id"), playlistId);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    SourceNml sourceNml;
    sourceNml.playlistId = playlistId;
    sourceNml.nmlPath = query.value(0).toString();
    sourceNml.uuid = query.value(1).toString();
    sourceNml.name = query.value(2).toString();
    return sourceNml;
}

void purgeOrphanSourceNml(const QSqlDatabase& database) {
    ensureSourceNmlTable(database);
    QSqlQuery query(database);
    query.exec(QStringLiteral(
            "DELETE FROM djnote_source_nml WHERE playlist_id NOT IN (SELECT id FROM Playlists)"));
}

SourceNmlKeeper::SourceNmlKeeper(
        PlaylistDAO* pPlaylistDao, const QSqlDatabase& database, QObject* parent)
        : QObject(parent),
          m_database(database) {
    purgeOrphanSourceNml(m_database);
    connect(pPlaylistDao,
            &PlaylistDAO::deleted,
            this,
            [this](int) { purgeOrphanSourceNml(m_database); });
}

} // namespace nml
} // namespace mixxx

#include "moc_sourcenml.cpp"
