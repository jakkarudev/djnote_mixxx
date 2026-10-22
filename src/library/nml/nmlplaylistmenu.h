#pragma once

#include <QObject>
#include <functional>

#include "preferences/usersettings.h"

class Library;
class PlaylistDAO;
class QMenu;
class QWidget;

// DJNote: what the Playlists library feature shows for the NML exchange. The
// feature's own code only calls these three functions (one marked line each),
// so the menus, the flows behind them and the Source NML bookkeeping stay here.

namespace mixxx {
namespace nml {

/// Creates the Source NML table if missing and drops the rows of deleted
/// Playlists; lives as long as `parent`.
void createSourceNmlKeeper(PlaylistDAO* pPlaylistDao, Library* pLibrary, QObject* parent);

/// "Import Traktor Playlist (.nml)..." for the Playlists root menu.
/// `activatePlaylist` is called with the id of the Playlist the import made.
void addNmlImportAction(QMenu* pMenu,
        QWidget* pParent,
        Library* pLibrary,
        UserSettingsPointer pConfig,
        std::function<void(int)> activatePlaylist);

/// "Export as Traktor Playlist (.nml)..." for a Playlist's menu, plus
/// "Re-read NML..." when the Playlist has a Source NML (disabled when locked).
void addNmlPlaylistActions(QMenu* pMenu,
        QWidget* pParent,
        Library* pLibrary,
        UserSettingsPointer pConfig,
        int playlistId,
        bool locked);

} // namespace nml
} // namespace mixxx
