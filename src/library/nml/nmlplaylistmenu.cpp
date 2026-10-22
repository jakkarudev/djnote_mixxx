#include "library/nml/nmlplaylistmenu.h"

#include <QMenu>

#include "library/dao/playlistdao.h"
#include "library/library.h"
#include "library/nml/nmlexportflow.h"
#include "library/nml/nmlimportflow.h"
#include "library/nml/nmlrereadflow.h"
#include "library/nml/nmlsupport.h"
#include "library/nml/sourcenml.h"
#include "library/trackcollectionmanager.h"

namespace mixxx {
namespace nml {

void createSourceNmlKeeper(PlaylistDAO* pPlaylistDao, Library* pLibrary, QObject* parent) {
    new SourceNmlKeeper(pPlaylistDao, nmlDatabase(pLibrary->trackCollectionManager()), parent);
}

void addNmlImportAction(QMenu* pMenu,
        QWidget* pParent,
        Library* pLibrary,
        UserSettingsPointer pConfig,
        std::function<void(int)> activatePlaylist) {
    // The menu label is the one the spec fixes; elsewhere the glossary's terms are used.
    pMenu->addAction(tr("Import Traktor Playlist (.nml)..."),
            pParent,
            [pParent, pLibrary, pConfig, activatePlaylist]() {
                const int playlistId = runNmlImportFlow(
                        pParent, pLibrary->trackCollectionManager(), pConfig);
                if (playlistId != kInvalidPlaylistId) {
                    activatePlaylist(playlistId);
                }
            });
}

void addNmlPlaylistActions(QMenu* pMenu,
        QWidget* pParent,
        Library* pLibrary,
        UserSettingsPointer pConfig,
        int playlistId,
        bool locked) {
    pMenu->addAction(tr("Export as Traktor Playlist (.nml)..."),
            pParent,
            [pParent, pLibrary, pConfig, playlistId]() {
                runNmlExportFlow(pParent, pLibrary->trackCollectionManager(), pConfig, playlistId);
            });
    // Only Playlists created by an NML import have a Source NML.
    if (loadSourceNml(nmlDatabase(pLibrary->trackCollectionManager()), playlistId)) {
        QAction* pRereadAction = pMenu->addAction(tr("Re-read NML..."),
                pParent,
                [pParent, pLibrary, pConfig, playlistId]() {
                    runNmlRereadFlow(
                            pParent, pLibrary->trackCollectionManager(), pConfig, playlistId);
                });
        pRereadAction->setEnabled(!locked);
    }
}

} // namespace nml
} // namespace mixxx
