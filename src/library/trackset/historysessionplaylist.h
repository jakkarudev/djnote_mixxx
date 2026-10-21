#pragma once

#include <QString>

class PlaylistDAO;
class QMenu;

// DJNote: "Save as Playlist" for a History session.

/// First free name of "<session> (Playlist)", "<session> (Playlist 2)", ...
QString suggestedPlaylistNameForSession(const PlaylistDAO& dao, const QString& sessionName);

/// Snapshots the tracks of a History session (in order, repeats kept) into a
/// new, visible Playlist. Returns its id or kInvalidPlaylistId.
int saveSessionAsPlaylist(PlaylistDAO& dao, int sessionId, const QString& name);

/// Adds the "Save as Playlist" action to the menu of a History session.
/// It is disabled while the session has no tracks.
void addSaveSessionAsPlaylistAction(QMenu* pMenu, PlaylistDAO* pDao, int sessionId);
